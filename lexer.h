/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 */
#ifndef LEXER_H
#define LEXER_H

typedef enum
{
    TOKEN_COMMAND,
    TOKEN_ARGUMENT,
    TOKEN_PIPE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_SEMICOLON,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_REDIRECT_OUT,    /* > */
    TOKEN_REDIRECT_IN,     /* < */
    TOKEN_REDIRECT_APPEND, /* >> */
    TOKEN_REDIRECT_ERR,    /* 2> */
    TOKEN_SUBSHELL,        /* $(...) */
    TOKEN_QUOTE,           /* '...' */
    TOKEN_AMPERSAND,       /* & */
    TOKEN_IF,              /* if */
    TOKEN_THEN,            /* then */
    TOKEN_ELSE,            /* else */
    TOKEN_FI,              /* fi */
    TOKEN_TEST,            /* test */
    TOKEN_LBRACKET,        /* [ */
    TOKEN_RBRACKET,        /* ] */
    TOKEN_FOR,             /* for */
    TOKEN_DO,              /* do */
    TOKEN_DONE,            /* done */
    TOKEN_WHILE,           /* while */
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct
{
    TokenType type;
    char *value;
} Token;

void lexer_init(const char *input);
Token lexer_next_token();
void free_token(Token token);

#endif /* LEXER_H */
