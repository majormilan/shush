#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "libtinyio/stdio.h"
#include "libtinyio/string.h"

static const char *input;
static size_t input_len;
static size_t pos;
static int debug_enabled = 0; // Debug flag

void lexer_init(const char *line) {
    input = line;
    input_len = strlen(line);
    pos = 0;
}

// Function to enable or disable debug messages
void lexer_set_debug(int enable) {
    debug_enabled = enable;
}

static void debug_print(const char *format, ...) {
    if (debug_enabled) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

static char peek() {
    return pos < input_len ? input[pos] : '\0';
}

static char advance() {
    return pos < input_len ? input[pos++] : '\0';
}

static Token make_token(TokenType type, const char *start, size_t length) {
    Token token;
    token.type = type;
    token.value = strndup(start, length);
    return token;
}

static Token make_error_token(const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.value = strdup(message);
    return token;
}

static Token make_command_or_arg_token(const char *start) {
    const char *initial = start; // Track the starting pointer
    while (!isspace(peek()) && peek() != '\0' && peek() != '|' && peek() != '&' && peek() != ';' && peek() != '(' && peek() != ')') {
        advance();
    }
    size_t length = input + pos - initial; // Correct length calculation
    return make_token(TOKEN_COMMAND, initial, length);
}

Token lexer_next_token() {
    while (isspace(peek())) advance();

    const char *start = input + pos;

    if (pos >= input_len) return make_token(TOKEN_EOF, "", 0);

    char c = advance();

    if (isalnum(c) || strchr("-$~", c)) {
        Token token = make_command_or_arg_token(start);
        debug_print("Lexer: Token type %d, value '%s'\n", token.type, token.value);
        return token;
    }

    Token token;
    switch (c) {
        case '|': token = peek() == '|' ? (advance(), make_token(TOKEN_OR, start, 2)) : make_token(TOKEN_PIPE, start, 1); break;
        case '&': token = peek() == '&' ? (advance(), make_token(TOKEN_AND, start, 2)) : make_error_token("Unexpected character '&'"); break;
        case ';': token = make_token(TOKEN_SEMICOLON, start, 1); break;
        case '(': token = make_token(TOKEN_LPAREN, start, 1); break;
        case ')': token = make_token(TOKEN_RPAREN, start, 1); break;
        case '"': {
            while (peek() != '"' && peek() != '\0') advance();
            if (peek() == '"') advance();
            token = make_token(TOKEN_COMMAND, start + 1, (input + pos) - start - 2); // Skip surrounding quotes
            break;
        }
        default:
            if (!isspace(c)) {
                token = make_command_or_arg_token(start);
                break;
            }
            token = make_error_token("Unrecognized character");
    }

    debug_print("Lexer: Token type %d, value '%s'\n", token.type, token.value);
    return token;
}

void free_token(Token token) {
    free(token.value);
}
