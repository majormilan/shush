#!/usr/bin/env bash
# Compile and run each unit test against the already-built project objects
# (run `make` first). Each test is a standalone binary with its own main();
# this script just builds and runs each one and aggregates pass/fail.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$ROOT/build"
TESTBIN="$BUILD/tests"
CC="${CC:-diet gcc}"
CFLAGS="-Wall -Os -I$ROOT"

mkdir -p "$TESTBIN"
fail=0

build_and_run() {
    local src="$1" name="$2"
    shift 2
    if $CC $CFLAGS "$ROOT/tests/unit/$src" "$@" -o "$TESTBIN/$name" 2>&1; then
        echo "--- $name ---"
        if ! "$TESTBIN/$name"; then
            fail=1
        fi
    else
        echo "BUILD FAILED: $src"
        fail=1
    fi
}

for obj in lexer builtins terminal init session; do
    if [ ! -f "$BUILD/obj/$obj.o" ]; then
        echo "missing $BUILD/obj/$obj.o -- run \`make\` first" >&2
        exit 1
    fi
done

build_and_run test_lexer.c test_lexer \
    "$BUILD/obj/lexer.o" -L"$BUILD/libtinyio" -ltinyio

build_and_run test_completion_utils.c test_completion_utils \
    "$BUILD/libtline/completion_utils.o" "$BUILD/libtline/utf8.o" \
    -L"$BUILD/libtinyio" -ltinyio

build_and_run test_glob.c test_glob \
    "$BUILD/obj/builtins.o" "$BUILD/obj/terminal.o" "$BUILD/obj/init.o" \
    "$BUILD/obj/lexer.o" "$BUILD/obj/session.o" \
    -L"$BUILD/libtline" -ltline -L"$BUILD/libtinyio" -ltinyio

exit $fail
