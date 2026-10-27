#!/usr/bin/env python3
"""
Differential tests: each case runs as a script under shush and under dash
(the reference POSIX sh), and stdout plus exit status must match. Cases
marked "nocrash" cover syntax shush does not support yet: there it only
has to fail cleanly instead of dying on a signal.
"""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SHUSH = os.environ.get("SHUSH", os.path.join(ROOT, "build", "shush"))
DASH = shutil.which("dash")

failures = []

# (name, script, how): how is "same", or "nocrash"
SCRIPTS = [
    ("$? after ; on one line", "false; echo $?\n", "same"),
    ("$? after || on one line", "true; false || echo $?\n", "same"),
    ("$? after a failed &&", "false && true; echo $?\n", "same"),
    ("$? of a pipeline is its last command's", "true | false; echo $?\n", "same"),
    ("command not found is 127", "nosuchcmd_xyz 2>/dev/null; echo $?\n", "same"),
    ("not executable is 126", "/etc/passwd 2>/dev/null; echo $?\n", "same"),
    ("killed by a signal is 128+n", "/bin/sh -c 'kill -TERM $$'; echo $?\n", "same"),
    ("the script's status is the last command's", "true\nfalse\n", "same"),
    ("a lone $ is literal", "echo $ a$ b\n", "same"),
    ("a long word after an expansion", "echo ${HOME}-" + "x" * 5000 + "\n", "same"),
    ("a long word after ~", "echo ~/" + "y" * 5000 + "\n", "same"),
    ("1500 arguments", "echo " + " ".join(["a"] * 1500) + "\n", "same"),
    ("${#x} does not crash", "echo ${#HOME}\n", "nocrash"),
    ("${? does not crash", "echo ${?}\n", "nocrash"),
    ("${ at the end does not crash", "echo ${\n", "nocrash"),
    ("fg without a job number", "sleep 0.1 &\nfg\necho done\n", "nocrash"),
]

# (name, argv after the shell, stdin, how)
INVOCATIONS = [
    ("-c runs its string", ["-c", "echo hi; false"], "", "same"),
    ("-c takes $0 and $1", ["-c", "echo $0 $1", "name", "arg"], "", "same"),
    ("-c with several lines", ["-c", "echo one\necho two"], "", "same"),
    ("-s reads stdin with arguments", ["-s", "x", "y"], "echo $1 $2\n", "same"),
    ("a script reads its arguments", None, None, "same"),
]


def run(shell, argv, stdin, env=None):
    try:
        r = subprocess.run([shell] + argv, input=stdin, capture_output=True,
                           text=True, timeout=10, env=env)
        return r.stdout, r.returncode
    except subprocess.TimeoutExpired:
        return None, "timeout"


def check(name, cond, detail=""):
    detail = detail if len(detail) <= 200 else detail[:200] + "..."
    print(f"[{'ok' if cond else 'FAIL'}] {name}" + (f" -- {detail}" if not cond else ""))
    if not cond:
        failures.append(name)


def compare(name, how, argv, stdin, env=None):
    got = run(SHUSH, argv, stdin, env)
    if how == "nocrash":
        check(name, got[1] != "timeout" and got[1] >= 0, f"shush: {got[1]}")
        return
    want = run(DASH, argv, stdin, env)
    check(name, got == want, f"shush {got!r}, dash {want!r}")


def main():
    if not DASH:
        print("dash not found: skipping differential tests")
        return 0
    with tempfile.TemporaryDirectory() as d:
        for i, (name, script, how) in enumerate(SCRIPTS):
            path = os.path.join(d, f"t{i}.sh")
            with open(path, "w") as f:
                f.write(script)
            compare(name, how, [path], "")
        path = os.path.join(d, "args.sh")
        with open(path, "w") as f:
            f.write("echo $0 $1 $2\n")
        for name, argv, stdin, how in INVOCATIONS:
            if argv is None:
                argv, stdin = [path, "p", "q"], ""
            compare(name, how, argv, stdin)
        env = {k: v for k, v in os.environ.items() if k != "HOME"}
        path = os.path.join(d, "home.sh")
        with open(path, "w") as f:
            f.write("echo ~\necho still here\n")
        compare("~ without HOME does not end the shell", "same", [path], "", env)
    print(f"\n{'PASSED' if not failures else 'FAILED'}: {len(failures)} failing test(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
