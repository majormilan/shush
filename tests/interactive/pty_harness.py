"""
Shared PTY driver for interactive shush tests (readline, tab completion --
anything that needs a real terminal, as opposed to tests/integration/, which
drives the binary over a plain pipe).

shush's readline queries the cursor position via the "\\x1b[6n" DSR escape
sequence and blocks on stdin waiting for a terminal to answer it; a bare PTY
with nothing on the other end never answers, so run_session() answers it
itself the moment it sees one, the same way a real terminal emulator would.
"""
import os
import pty
import select
import time


def run_session(binary, cmds, wait=0.25, settle=0.4, final_wait=0.3):
    """
    Fork `binary` (argv0 only, no args) under a PTY, feed it each byte
    string in `cmds` in order (pausing `wait` seconds after each one to let
    output settle), then send EOF (Ctrl-D) and return everything the child
    wrote to its PTY as bytes.
    """
    pid, fd = pty.fork()
    if pid == 0:
        os.execv(binary, [binary])
        os._exit(127)  # pragma: no cover - only reached if execv fails

    out = b""

    def pump(extra_wait):
        nonlocal out
        end = time.time() + extra_wait
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.1)
            if fd in r:
                try:
                    chunk = os.read(fd, 65536)
                except OSError:
                    break
                out += chunk
                if b"\x1b[6n" in chunk:
                    os.write(fd, b"\x1b[1;1R")
                end = time.time() + extra_wait  # keep draining while active

    try:
        pump(settle)
        for c in cmds:
            os.write(fd, c)
            pump(wait)
        os.write(fd, b"\x04")  # EOF
        pump(final_wait)
    finally:
        try:
            os.kill(pid, 9)
        except ProcessLookupError:
            pass
        try:
            os.waitpid(pid, 0)
        except ChildProcessError:
            pass
        os.close(fd)

    return out
