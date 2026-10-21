#!/usr/bin/env python3
"""
Integration tests for shush: drives the real built binary non-interactively
(plain stdin pipe, no PTY needed) and checks stdout/exit code. Covers
end-to-end behavior that doesn't depend on terminal semantics -- globbing,
redirection, job control, parsing -- as opposed to tests/interactive/, which
covers readline/completion and does need a PTY.
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SHUSH = os.path.join(ROOT, "build", "shush")

failures = []


def run(shell_input, cwd=None, timeout=5):
    return subprocess.run(
        [SHUSH],
        input=shell_input,
        cwd=cwd,
        capture_output=True,
        text=True,
        timeout=timeout,
    )


def check(name, cond, detail=""):
    status = "ok" if cond else "FAIL"
    print(f"[{status}] {name}" + (f" -- {detail}" if detail and not cond else ""))
    if not cond:
        failures.append(name)


def test_basic_exit_code():
    r = run("exit 7\n")
    check("exit code propagates", r.returncode == 7, f"got {r.returncode}")


def test_pipe_and_substitution():
    r = run("echo $(echo substituted)\n")
    check("command substitution", r.stdout.strip() == "substituted", r.stdout)

    r = run("echo a b c | wc -w\n")
    check("pipe to external command", r.stdout.strip() == "3", r.stdout)


def test_subshell_grouping():
    r = run("(echo in-subshell)\n")
    check("subshell grouping executes", "in-subshell" in r.stdout, r.stdout)


def test_quoted_trailing_spaces_preserved():
    # Regression: execute_ast used to trim_trailing_spaces() on quoted
    # arguments, silently eating spaces the user explicitly quoted.
    r = run('echo "foo   "\n')
    check(
        "quoted trailing spaces are preserved",
        r.stdout == "foo   \n",
        repr(r.stdout),
    )


def test_background_job_no_crash():
    # Regression: libtinyio's kill() called itself recursively, stack-
    # overflowing on every kill() call -- which fires on every background
    # job and on interactive Ctrl-C.
    r = run("sleep 0.2 &\nwait_dummy=1\n", timeout=5)
    check(
        "backgrounding a job does not crash",
        r.returncode != -11 and "Segmentation" not in r.stderr,
        f"returncode={r.returncode} stderr={r.stderr!r}",
    )


def test_bg_builtin_prints_correct_job_id():
    # Regression: tiny_vfprintf/tiny_vsnprintf didn't understand the 'l'
    # length modifier, so "%ld" (used by the bg builtin) misread the
    # va_list and printed garbage instead of the job id.
    r = run("sleep 1 &\nbg 1\n", timeout=5)
    check(
        "bg builtin prints the real job id, not garbage",
        "[1] " in r.stdout and "continued" in r.stdout,
        r.stdout,
    )


def test_external_test_bracket_command():
    # Regression: '[' and ']' used to tokenize as their own token types the
    # parser never consumed, so "[ ... ]" (external test/[) could never be
    # invoked at all.
    r = run("[ -f Makefile ] && echo yes-exists\n", cwd=ROOT)
    check("external test/[ works", r.stdout.strip() == "yes-exists", r.stdout)

    r = run("[ -f nonexistent_xyz_file ] && echo should-not-print\n", cwd=ROOT)
    check(
        "external test/[ correctly reports failure",
        r.stdout.strip() == "",
        r.stdout,
    )


def test_trailing_garbage_is_a_syntax_error():
    # Regression: parse() never checked the token stream was fully
    # consumed, so unparseable trailing input (e.g. a stray ')') was
    # silently dropped instead of reported.
    r = run("echo hi )\n")
    check(
        "unparseable trailing input is reported, not silently dropped",
        "Syntax error" in r.stderr and r.returncode != 0,
        f"stdout={r.stdout!r} stderr={r.stderr!r} rc={r.returncode}",
    )


def test_glob_expansion():
    with tempfile.TemporaryDirectory() as d:
        for name in ["a.txt", "b.txt", "c.log", "file1.txt", "file2.txt", ".hidden"]:
            open(os.path.join(d, name), "w").close()
        os.mkdir(os.path.join(d, "dirA"))
        os.mkdir(os.path.join(d, "dirB"))

        r = run("/bin/ls -d *.txt\n", cwd=d)
        got = sorted(r.stdout.split())
        check(
            "*.txt expands to the four .txt files",
            got == ["a.txt", "b.txt", "file1.txt", "file2.txt"],
            got,
        )

        r = run("/bin/ls -d [ab].txt\n", cwd=d)
        got = sorted(r.stdout.split())
        check("[ab].txt bracket class expands correctly", got == ["a.txt", "b.txt"], got)

        r = run("/bin/ls -d file?.txt\n", cwd=d)
        got = sorted(r.stdout.split())
        check("file?.txt single-char wildcard", got == ["file1.txt", "file2.txt"], got)

        r = run("/bin/ls -d d*\n", cwd=d)
        got = sorted(r.stdout.split())
        check("d* matches both directories", got == ["dirA", "dirB"], got)

        r = run("echo before [zz]*.nomatch after\n", cwd=d)
        check(
            "no-match glob falls back to literal, nothing else is dropped",
            r.stdout.strip() == "before [zz]*.nomatch after",
            repr(r.stdout),
        )


def test_redirection():
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "out.txt")
        run(f"echo one > {out}\necho two >> {out}\n", cwd=d)
        with open(out) as f:
            check("> then >> redirection", f.read() == "one\ntwo\n")

        run(f"echo hello > {out}\n", cwd=d)
        r = run(f"cat < {out}\n", cwd=d)
        check("< input redirection", r.stdout == "hello\n", r.stdout)


def test_fd_dup_redirects():
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "out.txt")

        # "2>&1" after the file redirect: stderr duplicates to the *now
        # redirected* stdout, so both error lines end up in the file.
        run(f"ls /nonexistent_xyz this_also_missing > {out} 2>&1\n", cwd=d)
        with open(out) as f:
            content = f.read()
        check(
            "N>&M after '> file': stderr merges into the file",
            content.count("No such file or directory") == 2,
            content,
        )

        # "2>&1" before the file redirect: stderr duplicates to the
        # *original* stdout (the terminal), so the file should stay empty --
        # order sensitivity, not just whether '2>&1' parses at all.
        open(out, "w").close()
        run(f"ls /nonexistent_xyz 2>&1 > {out}\n", cwd=d)
        with open(out) as f:
            check("N>&M before '> file': stderr does NOT end up in the file", f.read() == "")

        # >&2: redirect stdout to wherever stderr points.
        r = run("echo to-stderr >&2\n")
        check(">&2 moves stdout to stderr", r.stdout == "" and "to-stderr" in r.stderr, (r.stdout, r.stderr))

        # Malformed fd-dup target is a syntax error, not silently accepted.
        r = run("echo hi 2>&abc\n")
        check(
            "invalid fd-dup target is reported as a syntax error",
            "Syntax error" in r.stderr and r.returncode != 0,
            r.stderr,
        )


def test_redirect_both_and_herestring():
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "both.txt")
        run(f"ls /nonexistent_xyz &> {out}\n", cwd=d)
        with open(out) as f:
            check("&> merges stdout+stderr into a file", "No such file" in f.read())

        r = run('cat <<< "hello herestring"\n')
        check(
            "<<< herestring content, with bash's trailing newline",
            r.stdout == "hello herestring\n",
            repr(r.stdout),
        )

        # Heredocs remain genuinely unsupported, but now that '<<' actually
        # lexes (it used to be indistinguishable from two REDIRECT_IN
        # tokens and hit a generic parse error), this should be the
        # specific, honest rejection message instead.
        r = run("cat << EOF\nsome content\nEOF\n")
        check(
            "heredoc is honestly rejected, not silently mis-parsed",
            "here documents" in r.stderr and "not supported" in r.stderr,
            r.stderr,
        )


def test_and_or_short_circuit():
    r = run("true && echo and-ran\n")
    check("&& runs on success", "and-ran" in r.stdout)

    r = run("false && echo should-not-print\n")
    check("&& skips on failure", "should-not-print" not in r.stdout)

    r = run("false || echo or-ran\n")
    check("|| runs on failure", "or-ran" in r.stdout)


def test_alias():
    r = run("alias greet='echo hi'\ngreet\nunalias greet\ngreet\n")
    lines = r.stdout.splitlines()
    check("alias expands", "hi" in lines)
    check(
        "unalias removes it (falls through to command-not-found)",
        "hi" in r.stderr or "command not found" in r.stderr or len(lines) == 1,
        r.stderr,
    )


def main():
    if not os.path.isfile(SHUSH):
        print(f"shush binary not found at {SHUSH}; run `make` first", file=sys.stderr)
        return 1

    test_basic_exit_code()
    test_pipe_and_substitution()
    test_subshell_grouping()
    test_quoted_trailing_spaces_preserved()
    test_background_job_no_crash()
    test_bg_builtin_prints_correct_job_id()
    test_external_test_bracket_command()
    test_trailing_garbage_is_a_syntax_error()
    test_glob_expansion()
    test_redirection()
    test_fd_dup_redirects()
    test_redirect_both_and_herestring()
    test_and_or_short_circuit()
    test_alias()

    total = len(failures)
    print(f"\n{'FAILED' if total else 'PASSED'}: {total} failing test(s)")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
