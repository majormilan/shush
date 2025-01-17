/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 */
#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TOKEN_COMMAND,
    TOKEN_ARGUMENT,
    TOKEN_PIPE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_SEMICOLON,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char *value;
} Token;

void lexer_init(const char *input);
Token lexer_next_token();
void free_token(Token token);

#endif /* LEXER_H */
