#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

/* Token types */
typedef enum {
    TOKEN_COMMAND,      /* Command or argument */
    TOKEN_PIPE,         /* | */
    TOKEN_AND,          /* && */
    TOKEN_OR,           /* || */
    TOKEN_SEMICOLON,    /* ; */
    TOKEN_REDIRECT_OUT, /* > */
    TOKEN_REDIRECT_IN,  /* < */
    TOKEN_REDIRECT_APPEND, /* >> */
    TOKEN_REDIRECT_ERR, /* 2> */
    TOKEN_REDIRECT_BOTH, /* &> */
    TOKEN_HEREDOC,      /* << */
    TOKEN_HERESTRING,   /* <<< */
    TOKEN_LPAREN,       /* ( */
    TOKEN_RPAREN,       /* ) */
    TOKEN_AMPERSAND,    /* & */
    TOKEN_LBRACKET,     /* [ */
    TOKEN_RBRACKET,     /* ] */
    TOKEN_SUBSHELL,     /* $(...) */
    TOKEN_QUOTE,        /* '...' */
    TOKEN_STRING,       /* "..." */
    TOKEN_ERROR,        /* Error token */
    TOKEN_EOF           /* End of input */
} TokenType;

/* Token structure */
typedef struct {
    TokenType type;
    char *value;
} Token;

/* Function prototypes */
void lexer_init(const char *line);
Token lexer_next_token(void);
void free_token(Token token);

#endif /* LEXER_H */
