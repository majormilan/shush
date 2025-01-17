#include "builtins.h"
#include "parse.h"
#include "lexer.h"
#include "libtinyio/stdio.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static Token current_token;

// Utility function for variable expansion
static size_t append_env_var(char **res, size_t *res_len, const char *env_name) {
    const char *end = env_name;

    while (*end && (isalnum(*end) || *end == '_'))
        end++;

    size_t name_len = end - env_name;
    if (name_len) {
        char var[name_len + 1];
        strncpy(var, env_name, name_len);
        var[name_len] = '\0';

        char *val = getenv(var);
        if (val) {
            size_t val_len = strlen(val);
            *res = realloc(*res, *res_len + val_len + 1);
            if (!*res) {
                perror("realloc");
                exit(1);
            }
            strcpy(*res + *res_len, val);
            *res_len += val_len;
        }
    }
    return name_len;
}

// Function to expand variables
char *expand_variables(const char *input) {
    size_t len = strlen(input);
    char *res = malloc(len + 1);
    if (!res) {
        perror("malloc");
        exit(1);
    }

    size_t res_len = 0;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '~') {
            const char *home = getenv("HOME");
            if (!home) {
                fprintf(stderr, "Error: HOME not set\n");
                exit(1);
            }
            size_t home_len = strlen(home);
            res = realloc(res, res_len + home_len + 1);
            if (!res) {
                perror("realloc");
                exit(1);
            }
            strcpy(res + res_len, home);
            res_len += home_len;
        } else if (input[i] == '$' && i + 1 < len) {
            i += append_env_var(&res, &res_len, input + i + 1);
        } else {
            res[res_len++] = input[i];
        }
    }

    res[res_len] = '\0';
    return res;
}

static ASTNode *create_ast_node(TokenType type, const char *value) {
    ASTNode *node = malloc(sizeof(ASTNode));
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->left = NULL;
    node->right = NULL;
    return node;
}

static void syntax_error(const char *message) {
    fprintf(stderr, "Syntax error: %s\n", message);
    exit(1);
}

static void consume_token(TokenType type) {
    if (current_token.type == type) {
        free_token(current_token);
        current_token = lexer_next_token();
    } else {
        syntax_error("Unexpected token");
    }
}

static ASTNode *parse_command() {
    if (current_token.type == TOKEN_COMMAND) {
        ASTNode *node = create_ast_node(TOKEN_COMMAND, current_token.value);
        consume_token(TOKEN_COMMAND);
        while (current_token.type == TOKEN_COMMAND) {
            ASTNode *arg_node = create_ast_node(TOKEN_COMMAND, current_token.value);
            ASTNode *temp = node;
            while (temp->right) temp = temp->right;
            temp->right = arg_node;
            consume_token(TOKEN_COMMAND);
        }
        return node;
    }
    syntax_error("Expected command");
    return NULL;
}

static ASTNode *parse_expression();

static ASTNode *parse_parentheses() {
    consume_token(TOKEN_LPAREN);
    ASTNode *node = parse_expression();
    consume_token(TOKEN_RPAREN);
    return node;
}

static ASTNode *parse_factor() {
    if (current_token.type == TOKEN_COMMAND) {
        return parse_command();
    } else if (current_token.type == TOKEN_LPAREN) {
        return parse_parentheses();
    }
    syntax_error("Expected factor");
    return NULL;
}

static ASTNode *parse_term() {
    ASTNode *node = parse_factor();

    while (current_token.type == TOKEN_PIPE) {
        TokenType op = current_token.type;
        consume_token(op);
        ASTNode *right = parse_factor();
        ASTNode *new_node = create_ast_node(op, NULL);
        new_node->left = node;
        new_node->right = right;
        node = new_node;
    }

    return node;
}

static ASTNode *parse_expression() {
    ASTNode *node = parse_term();

    while (current_token.type == TOKEN_AND || current_token.type == TOKEN_OR || current_token.type == TOKEN_SEMICOLON) {
        TokenType op = current_token.type;
        consume_token(op);
        ASTNode *right = parse_term();
        ASTNode *new_node = create_ast_node(op, NULL);
        new_node->left = node;
        new_node->right = right;
        node = new_node;
    }

    return node;
}

ASTNode *parse() {
    current_token = lexer_next_token();
    if (current_token.type == TOKEN_EOF) {
        return NULL;
    }
    return parse_expression();
}

void free_ast(ASTNode *root) {
    if (root) {
        free_ast(root->left);
        free_ast(root->right);
        free(root->value);
        free(root);
    }
}

void print_syntax_error(const char *message) {
    fprintf(stderr, "Syntax error: %s\n", message);
}


int execute_ast(ASTNode *root) {
    if (root == NULL) {
        return 0;
    }

    int left_status = 0;
    int right_status = 0;

    switch (root->type) {
        case TOKEN_COMMAND: {
            char *args[1024];
            int i = 0;
            ASTNode *temp = root;
            while (temp) {
                args[i++] = expand_variables(temp->value);
                temp = temp->right;
            }
            args[i] = NULL;
            return exec_command(args[0], args);
        }
        case TOKEN_PIPE: {
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                perror("pipe");
                return 1;
            }

            pid_t pid = fork();
            if (pid == 0) {
                // Child process: Executes the left side of the pipe
                close(pipefd[0]); // Close unused read end
                dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe write end
                close(pipefd[1]); // Close write end after duplicating
                int status = execute_ast(root->left);
                _exit(status); // Use _exit to terminate child process
            } else if (pid < 0) {
                perror("fork");
                return 1;
            } else {
                // Parent process: Executes the right side of the pipe
                close(pipefd[1]); // Close unused write end
                int saved_stdin = dup(STDIN_FILENO); // Save current stdin
                dup2(pipefd[0], STDIN_FILENO); // Redirect stdin to pipe read end
                close(pipefd[0]); // Close read end after duplicating
                right_status = execute_ast(root->right); // Execute right side
                dup2(saved_stdin, STDIN_FILENO); // Restore original stdin
                close(saved_stdin); // Close saved stdin descriptor
                waitpid(pid, &left_status, 0); // Wait for the left side to finish
            }

            return right_status;
        }
        case TOKEN_AND:
            left_status = execute_ast(root->left);
            if (left_status == 0) {
                return execute_ast(root->right);
            }
            return left_status;
        case TOKEN_OR:
            left_status = execute_ast(root->left);
            if (left_status != 0) {
                return execute_ast(root->right);
            }
            return left_status;
        case TOKEN_SEMICOLON:
            execute_ast(root->left);
            return execute_ast(root->right);
        default:
            fprintf(stderr, "Unknown AST node type\n");
            return 1;
    }

    return 0;
}

int exec_command(char *cmd, char **args) {
    if (is_builtin(cmd)) {
        return run_builtin(args);
    }

    pid_t pid = fork();
    if (pid == 0) {
        // In the child process
        execvp(cmd, args);
        perror("execvp");
        _exit(1); // Use _exit instead of exit
    } else if (pid < 0) {
        perror("fork");
        return -1; // Return -1 to indicate fork failure
    } else {
        // In the parent process
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
}
void parse_and_execute(char *line) {
    lexer_init(line);
    ASTNode *root = parse();
    if (root) {
        execute_ast(root);
        free_ast(root);
    }
}
