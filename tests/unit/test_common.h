/*
 * Minimal, dependency-free C test assertion helpers.
 *
 * Deliberately not a framework: each test .c is a standalone program with
 * its own main(). Include this header, write test_xxx() functions that call
 * the CHECK_* macros, call them from main(), then TEST_SUMMARY() at the end.
 * main() returns nonzero if anything failed, so it composes directly with
 * `make test` / shell '&&' chains without any separate runner logic.
 */
#ifndef TEST_COMMON_H
#define TEST_COMMON_H

/*
 * The project's own files never call the real libc's printf/fopen/strcmp
 * family directly -- they go through libtinyio's macro remap to the tiny_*
 * implementations, which also redefine FILE as an incompatible struct from
 * dietlibc's own. Anything these tests link against (lexer.o,
 * completion_utils.o, ...) was compiled under that remap, so the test
 * binaries have to use it too: mixing the two FILE/stdin definitions in one
 * binary is a hard link error, not just a style mismatch.
 */
#include "../../libtinyio/stdio.h"
#include "../../libtinyio/string.h"

static int test_failures = 0;
static int test_count = 0;

#define CHECK(cond, label)                                                   \
    do {                                                                     \
        test_count++;                                                       \
        if (!(cond)) {                                                      \
            test_failures++;                                                \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, label);  \
        }                                                                    \
    } while (0)

#define CHECK_EQ_INT(actual, expected, label)                                \
    do {                                                                     \
        test_count++;                                                       \
        long _a = (long)(actual), _e = (long)(expected);                    \
        if (_a != _e) {                                                     \
            test_failures++;                                                \
            fprintf(stderr, "FAIL %s:%d: %s (got %ld, expected %ld)\n",      \
                    __FILE__, __LINE__, label, _a, _e);                     \
        }                                                                    \
    } while (0)

#define CHECK_EQ_STR(actual, expected, label)                                \
    do {                                                                     \
        test_count++;                                                       \
        const char *_a = (actual), *_e = (expected);                        \
        int _ok = (_a && _e && strcmp(_a, _e) == 0);                        \
        if (!_ok) {                                                         \
            test_failures++;                                                \
            fprintf(stderr, "FAIL %s:%d: %s (got \"%s\", expected \"%s\")\n",\
                    __FILE__, __LINE__, label, _a ? _a : "(null)",           \
                    _e ? _e : "(null)");                                    \
        }                                                                   \
    } while (0)

#define CHECK_NULL(ptr, label)                                               \
    do {                                                                     \
        test_count++;                                                       \
        if ((ptr) != NULL) {                                                \
            test_failures++;                                                \
            fprintf(stderr, "FAIL %s:%d: %s (expected NULL)\n", __FILE__,    \
                    __LINE__, label);                                       \
        }                                                                   \
    } while (0)

#define TEST_SUMMARY()                                                       \
    do {                                                                     \
        printf("%s: %d/%d passed\n", __FILE__, test_count - test_failures,   \
               test_count);                                                 \
        return test_failures ? 1 : 0;                                       \
    } while (0)

#endif /* TEST_COMMON_H */
