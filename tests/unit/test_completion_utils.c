/*
 * Unit tests for libtline/completion_utils.c's find_command_start/
 * extract_command: the fix for compound-command completion ("cd /tmp &&
 * ech<TAB>" completing against the wrong command).
 */
#include "../../libtline/completion_utils.h"
#include "test_common.h"
#include <stdlib.h>
#include <string.h>

static void check_start(const char *buf, size_t cursor, size_t expected,
                        const char *label)
{
    CHECK_EQ_INT(find_command_start(buf, cursor), (long)expected, label);
}

static void test_find_command_start(void)
{
    check_start("ls -la", 6, 0, "no separator: whole buffer is one command");
    check_start("ls | grep foo", 6, 5, "after pipe: starts at 'grep'");
    check_start("echo hi && gi", 13, 11, "after &&: starts mid-word at 'gi'");
    check_start("cmd1 ; cmd2", 11, 7, "after semicolon");
    check_start("sleep 5 & ec", 12, 10, "after background &");
    check_start("(ls", 3, 1, "after open paren");
    check_start("{ls", 3, 1, "after open brace");
    check_start("echo \"a ; b\" c", 13, 0, "semicolon inside double quotes ignored");
    check_start("echo 'a && b' c", 14, 0, "&& inside single quotes ignored");
}

static void check_extract(const char *buf, size_t cursor, const char *expected,
                          const char *label)
{
    char *got = extract_command(buf, cursor);
    CHECK_EQ_STR(got, expected, label);
    free(got);
}

static void test_extract_command(void)
{
    check_extract("ls | gre", 8, "gre", "mid-completion after pipe");
    check_extract("ls | grep foo", 13, "grep", "argument position: command is grep, not ls");
    check_extract("cd /tmp && ech", 14, "ech", "compound command: second command, not cd");
}

int main(void)
{
    test_find_command_start();
    test_extract_command();
    TEST_SUMMARY();
}
