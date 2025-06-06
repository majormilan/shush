/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 */
#ifndef PARSE_H
#define PARSE_H

#include "lexer.h"
#include <stdbool.h>
#include <sys/types.h>

typedef struct ASTNode
{
    TokenType type;
    char *value;
    struct ASTNode *left;
    struct ASTNode *right;
    int redirect_fd;      /* File descriptor for redirection (e.g., 1 for stdout, 2 for stderr) */
    char *redirect_file;  /* Filename for redirection */
    bool background;      /* Flag for background execution */
} ASTNode;


#define MAX_BG_PROCS 100
extern pid_t bg_procs[MAX_BG_PROCS]; /* Array to store background process IDs */
extern int bg_proc_count;            /* Number of active background processes */



void parse_and_execute(char *line);
char *expand_variables(const char *input, TokenType token_type);
ASTNode *parse();
void free_ast(ASTNode *root);
void print_syntax_error(const char *message);
int execute_ast(ASTNode *root);
int exec_command(char *cmd, char **args);

#endif /* PARSE_H */
