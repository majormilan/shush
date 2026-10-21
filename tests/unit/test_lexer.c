/* Unit tests for lexer.c: lexer_init/lexer_next_token token-stream checks. */
#include "../../lexer.h"
#include "test_common.h"
#include <stdlib.h>

static Token next(void) { return lexer_next_token(); }

static void expect_token(TokenType type, const char *value, const char *label)
{
    Token t = next();
    CHECK_EQ_INT(t.type, type, label);
    if (value)
    {
        CHECK_EQ_STR(t.value, value, label);
    }
    free_token(t);
}

static void test_simple_command(void)
{
    lexer_init("ls -la");
    expect_token(TOKEN_COMMAND, "ls", "simple_command: first word");
    expect_token(TOKEN_COMMAND, "-la", "simple_command: second word");
    expect_token(TOKEN_EOF, NULL, "simple_command: eof");
}

static void test_operators(void)
{
    lexer_init("a | b && c || d ; e & f");
    expect_token(TOKEN_COMMAND, "a", "operators: a");
    expect_token(TOKEN_PIPE, NULL, "operators: pipe");
    expect_token(TOKEN_COMMAND, "b", "operators: b");
    expect_token(TOKEN_AND, NULL, "operators: and");
    expect_token(TOKEN_COMMAND, "c", "operators: c");
    expect_token(TOKEN_OR, NULL, "operators: or");
    expect_token(TOKEN_COMMAND, "d", "operators: d");
    expect_token(TOKEN_SEMICOLON, NULL, "operators: semicolon");
    expect_token(TOKEN_COMMAND, "e", "operators: e");
    expect_token(TOKEN_AMPERSAND, NULL, "operators: ampersand");
    expect_token(TOKEN_COMMAND, "f", "operators: f");
    expect_token(TOKEN_EOF, NULL, "operators: eof");
}

static void test_redirects(void)
{
    lexer_init("a > b >> c < d");
    expect_token(TOKEN_COMMAND, "a", "redirects: a");
    expect_token(TOKEN_REDIRECT_OUT, NULL, "redirects: >");
    expect_token(TOKEN_COMMAND, "b", "redirects: b");
    expect_token(TOKEN_REDIRECT_APPEND, NULL, "redirects: >>");
    expect_token(TOKEN_COMMAND, "c", "redirects: c");
    expect_token(TOKEN_REDIRECT_IN, NULL, "redirects: <");
    expect_token(TOKEN_COMMAND, "d", "redirects: d");
}

static void test_io_number(void)
{
    lexer_init("cmd 2> file");
    expect_token(TOKEN_COMMAND, "cmd", "io_number: cmd");
    expect_token(TOKEN_IO_NUMBER, "2", "io_number: 2");
    expect_token(TOKEN_REDIRECT_OUT, NULL, "io_number: >");
    expect_token(TOKEN_COMMAND, "file", "io_number: file");
}

static void test_fd_dup_redirects(void)
{
    /* 2>&1: IO_NUMBER "2", REDIRECT_OUT, AMPERSAND, then "1" -- the parser
     * accepts "1" as either COMMAND or IO_NUMBER, since the lexer is
     * context-free and can't know "1" isn't itself a prefix for some later
     * redirect (see test_fd_dup_ambiguous_io_number below). */
    lexer_init("2>&1");
    expect_token(TOKEN_IO_NUMBER, "2", "fd_dup: 2");
    expect_token(TOKEN_REDIRECT_OUT, NULL, "fd_dup: >");
    expect_token(TOKEN_AMPERSAND, NULL, "fd_dup: &");
    expect_token(TOKEN_COMMAND, "1", "fd_dup: 1 (plain command, nothing follows)");
    expect_token(TOKEN_EOF, NULL, "fd_dup: eof");
}

static void test_fd_dup_ambiguous_io_number(void)
{
    /* In "2>&1 > file", the '1' is itself immediately followed by another
     * '>', so the lexer's context-free IO_NUMBER lookahead classifies it as
     * TOKEN_IO_NUMBER, not TOKEN_COMMAND -- the parser has to accept both. */
    lexer_init("2>&1 > file");
    expect_token(TOKEN_IO_NUMBER, "2", "ambiguous: 2");
    expect_token(TOKEN_REDIRECT_OUT, NULL, "ambiguous: >");
    expect_token(TOKEN_AMPERSAND, NULL, "ambiguous: &");
    expect_token(TOKEN_IO_NUMBER, "1", "ambiguous: 1 classified as IO_NUMBER, not COMMAND");
    expect_token(TOKEN_REDIRECT_OUT, NULL, "ambiguous: second >");
    expect_token(TOKEN_COMMAND, "file", "ambiguous: file");
}

static void test_redirect_both_and_herestring(void)
{
    lexer_init("cmd &> file");
    expect_token(TOKEN_COMMAND, "cmd", "redirect_both: cmd");
    expect_token(TOKEN_REDIRECT_BOTH, NULL, "redirect_both: &>");
    expect_token(TOKEN_COMMAND, "file", "redirect_both: file");

    lexer_init("cmd <<< text");
    expect_token(TOKEN_COMMAND, "cmd", "herestring: cmd");
    expect_token(TOKEN_HERESTRING, NULL, "herestring: <<<");
    expect_token(TOKEN_COMMAND, "text", "herestring: text");

    lexer_init("cmd << EOF");
    expect_token(TOKEN_COMMAND, "cmd", "heredoc: cmd");
    expect_token(TOKEN_HEREDOC, NULL, "heredoc: <<");
    expect_token(TOKEN_COMMAND, "EOF", "heredoc: delimiter");
}

static void test_quotes(void)
{
    lexer_init("'single' \"double\"");
    expect_token(TOKEN_QUOTE, "single", "quotes: single-quoted");
    expect_token(TOKEN_STRING, "double", "quotes: double-quoted");
}

static void test_subshell(void)
{
    lexer_init("$(echo hi)");
    expect_token(TOKEN_SUBSHELL, "echo hi", "subshell: command substitution");
}

static void test_brackets_are_literal_word_chars(void)
{
    /* Regression: '[' and ']' used to tokenize as their own
     * TOKEN_LBRACKET/TOKEN_RBRACKET types, which the parser never consumed,
     * silently truncating any word containing them (e.g. glob patterns like
     * "[ab].txt"). They must now lex as ordinary word characters. */
    lexer_init("[ab].txt");
    expect_token(TOKEN_COMMAND, "[ab].txt", "brackets: glob pattern stays one word");

    lexer_init("[ -f foo ]");
    expect_token(TOKEN_COMMAND, "[", "brackets: bare '[' is a command word");
    expect_token(TOKEN_COMMAND, "-f", "brackets: -f");
    expect_token(TOKEN_COMMAND, "foo", "brackets: foo");
    expect_token(TOKEN_COMMAND, "]", "brackets: bare ']' is a command word");
    expect_token(TOKEN_EOF, NULL, "brackets: eof reached, nothing dropped");
}

int main(void)
{
    test_simple_command();
    test_operators();
    test_redirects();
    test_io_number();
    test_fd_dup_redirects();
    test_fd_dup_ambiguous_io_number();
    test_redirect_both_and_herestring();
    test_quotes();
    test_subshell();
    test_brackets_are_literal_word_chars();
    TEST_SUMMARY();
}
