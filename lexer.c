#include "lexer.h"
#include "libtinyio/stdio.h"
#include "libtinyio/string.h"
#include <ctype.h>
#include <stdlib.h>

static const char *input;
static size_t input_len;
static size_t pos;

/* Initialize lexer with the input line */
void lexer_init(const char *line)
{
    input = line;
    input_len = strlen(line);
    pos = 0;
}

/* Peek at the current character without advancing */
static char peek() { return pos < input_len ? input[pos] : '\0'; }

/* Advance to the next character */
static char advance() { return pos < input_len ? input[pos++] : '\0'; }

/* Create a token with the given type and value */
static Token make_token(TokenType type, const char *start, size_t length)
{
    Token token;
    token.type = type;
    token.value = strndup(start, length);
    return token;
}

/* Create an error token with the given message */
static Token make_error_token(const char *message)
{
    Token token;
    token.type = TOKEN_ERROR;
    token.value = strdup(message);
    return token;
}

/* Create a command or argument token */
static Token make_command_or_arg_token(const char *start)
{
    const char *initial = start;
    while (!isspace(peek()) && peek() != '\0' &&
           strchr("|&;()><\"'[]", peek()) == NULL)
    {
        advance();
    }
    size_t length = input + pos - initial;
    return make_token(TOKEN_COMMAND, initial, length);
}

/* Get the next token from the input */
Token lexer_next_token()
{
    while (isspace(peek()))
    {
        advance();
    }

    const char *start = input + pos;

    if (pos >= input_len)
    {
        return make_token(TOKEN_EOF, "", 0);
    }

    char c = advance();

    if (isalnum(c) || strchr("-~", c))
    {
        return make_command_or_arg_token(start);
    }

    Token token;
    switch (c)
    {
        case '|':
            token = peek() == '|' ? (advance(), make_token(TOKEN_OR, start, 2))
                                  : make_token(TOKEN_PIPE, start, 1);
            break;
        case '&':
            token = peek() == '&'
                        ? (advance(), make_token(TOKEN_AND, start, 2))
                        : make_token(TOKEN_AMPERSAND, start, 1);
            break;
        case ';':
            token = make_token(TOKEN_SEMICOLON, start, 1);
            break;
        case '$':
            if (peek() == '(')
            {
                advance(); /* Skip '(' */
                int paren_count = 1;
                const char *subshell_start = input + pos;
                while (peek() && paren_count > 0)
                {
                    char ch = advance();
                    if (ch == '(')
                        paren_count++;
                    else if (ch == ')')
                        paren_count--;
                }
                if (paren_count > 0)
                    return make_error_token("Unclosed subshell");
                size_t length = (input + pos - 1) - subshell_start;
                token = make_token(TOKEN_SUBSHELL, subshell_start, length);
            }
            else
            {
                return make_command_or_arg_token(start);
            }
            break;
        case '(':
            token = make_token(TOKEN_LPAREN, start, 1);
            break;
        case ')':
            token = make_token(TOKEN_RPAREN, start, 1);
            break;
        case '>':
            token = peek() == '>'
                        ? (advance(), make_token(TOKEN_REDIRECT_APPEND, start, 2))
                        : make_token(TOKEN_REDIRECT_OUT, start, 1);
            break;
        case '<':
            token = make_token(TOKEN_REDIRECT_IN, start, 1);
            break;
        case '2':
            if (peek() == '>')
            {
                advance();
                token = make_token(TOKEN_REDIRECT_ERR, start, 2);
            }
            else
            {
                return make_command_or_arg_token(start);
            }
            break;
        case '[':
            token = make_token(TOKEN_LBRACKET, start, 1);
            break;
        case ']':
            token = make_token(TOKEN_RBRACKET, start, 1);
            break;
        case '\"':
        {
            while (peek() != '\"' && peek() != '\0')
            {
                advance();
            }
            if (peek() == '\"')
            {
                advance();
                token = make_token(TOKEN_COMMAND, start + 1,
                                   (input + pos) - start - 2);
            }
            else
            {
                token = make_error_token("Unclosed double quote");
            }
            break;
        }
        case '\'':
        {
            while (peek() != '\'' && peek() != '\0')
            {
                advance();
            }
            if (peek() == '\'')
            {
                advance();
                token = make_token(TOKEN_QUOTE, start + 1,
                                   (input + pos) - start - 2);
            }
            else
            {
                token = make_error_token("Unclosed single quote");
            }
            break;
        }
        default:
            token = !isspace(c) ? make_command_or_arg_token(start)
                                : make_error_token("Unrecognized character");
    }

    return token;
}

/* Free the memory allocated for a token */
void free_token(Token token) { free(token.value); }
