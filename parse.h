/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 */
#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>
#include "lexer.h"

typedef struct ASTNode {
    TokenType type;
    char *value;
    struct ASTNode *left;
    struct ASTNode *right;
} ASTNode;

void parse_and_execute(char *line);
char *expand_variables(const char *input);
ASTNode *parse();
void free_ast(ASTNode *root);
void print_syntax_error(const char *message);
int execute_ast(ASTNode *root);
int exec_command(char *cmd, char **args);

#endif /* PARSE_H */
