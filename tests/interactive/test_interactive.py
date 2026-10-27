#!/usr/bin/env python3
"""
Interactive (PTY-driven) tests: readline and tab completion, which need a
real terminal and can't be exercised over a plain pipe. See
tests/integration/ for everything else.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pty_harness import run_session

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SHUSH = os.environ.get("SHUSH", os.path.join(ROOT, "build", "shush"))

failures = []


def check(name, cond, detail=""):
    status = "ok" if cond else "FAIL"
    print(f"[{status}] {name}" + (f" -- {detail}" if detail and not cond else ""))
    if not cond:
        failures.append(name)


def test_compound_command_completion():
    # Regression: is_command_position()/extract_command() used to scan
    # from buffer index 0 always, so completion after '&&'/'|'/';' used the
    # first command's context instead of the current one -- "cd /tmp &&
    # ech<TAB>" tried to complete "ech" as a cd path argument instead of a
    # command name.
    out = run_session(SHUSH, [b"cd /tmp && ech", b"\t"])
    text = out.decode(errors="replace")
    check(
        "completion after '&&' uses command context, not the first command's",
        "cd /tmp && echo" in text,
        text[-200:],
    )


def test_alias_completion():
    # Regression: builtins.c's builtin_completion (alias names, kill PIDs)
    # was written but never wired into completion_callbacks[], so it could
    # never fire.
    out = run_session(
        SHUSH, [b"alias foobar123='ls -la'\r", b"unalias foo", b"\t"]
    )
    text = out.decode(errors="replace")
    check(
        "alias names complete via the registered callback",
        "foobar123" in text and "unalias foobar123" in text,
        text[-300:],
    )


def test_double_tab_listing_no_hang():
    # Regression: the multi-candidate listing path had a redundant manual
    # prompt reprint that left a stray duplicate prompt line on screen;
    # also a basic liveness check that double-Tab doesn't hang.
    out = run_session(SHUSH, [b"ls", b"\t", b"\t"], wait=0.4)
    text = out.decode(errors="replace")
    check(
        "double-Tab produces a listing without hanging",
        len(text) > 50 and "ls" in text,
        text[-300:],
    )


def test_typeahead_kept():
    # Regression: raw mode was switched with TCSAFLUSH, which drops input
    # typed but not read yet; the cursor position query ate keystrokes
    # too. Typed fast (or on a serial console) the second command lost its
    # beginning.
    out = run_session(SHUSH, [b"echo one1\recho two2\r"], wait=0.5)
    text = out.decode(errors="replace")
    lines = [l.strip() for l in text.replace("\r", "\n").split("\n")]
    check("two commands typed at once both run",
          "one1" in lines and "two2" in lines, text[-300:])


def test_syntax_error_in_substitution():
    # Regression: a syntax error inside $(...) longjmp'd to the
    # interactive loop in the forked child, which then read the terminal
    # as a second shell with its stdout in the pipe.
    out = run_session(SHUSH, [b"echo $(echo ;)\r", b"echo after1\r"], wait=0.5)
    text = out.decode(errors="replace")
    lines = [l.strip() for l in text.replace("\r", "\n").split("\n")]
    check("a syntax error in $(...) leaves no second shell behind",
          "after1" in lines, text[-300:])


def main():
    if not os.path.isfile(SHUSH):
        print(f"shush binary not found at {SHUSH}; run `make` first", file=sys.stderr)
        return 1

    test_compound_command_completion()
    test_alias_completion()
    test_double_tab_listing_no_hang()
    test_typeahead_kept()
    test_syntax_error_in_substitution()

    total = len(failures)
    print(f"\n{'FAILED' if total else 'PASSED'}: {total} failing test(s)")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
