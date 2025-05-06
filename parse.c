#include "parse.h"
#include "builtins.h"
#include "lexer.h"
#include "libtinyio/stdio.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <glob.h>
#include <setjmp.h>

#define MAX_PATH_LEN 4096

static Token current_token;
static jmp_buf parse_recovery;

/* Utility function for variable expansion */
static size_t append_env_var(char **res, size_t *res_len, const char *env_name)
{
    const char *end = env_name;
    while (*end && (isalnum(*end) || *end == '_'))
        end++;
    size_t name_len = end - env_name;
    if (name_len)
    {
        char var[name_len + 1];
        strncpy(var, env_name, name_len);
        var[name_len] = '\0';
        char *val = getenv(var);
        if (val)
        {
            size_t val_len = strlen(val);
            *res = realloc(*res, *res_len + val_len + 1);
            if (!*res)
            {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            strcpy(*res + *res_len, val);
            *res_len += val_len;
        }
    }
    return name_len;
}

/* Function to expand variables */
char *expand_variables(const char *input, TokenType token_type)
{
    if (token_type == TOKEN_QUOTE)
    {
        return strdup(input); /* No expansion for single quotes */
    }

    size_t len = strlen(input);
    char *res = malloc(len + 1);
    if (!res)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    size_t res_len = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (input[i] == '~')
        {
            const char *home = getenv("HOME");
            if (!home)
            {
                fprintf(stderr, "Error: HOME not set\n");
                exit(EXIT_FAILURE);
            }
            size_t home_len = strlen(home);
            res = realloc(res, res_len + home_len + 1);
            if (!res)
            {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            strcpy(res + res_len, home);
            res_len += home_len;
        }
        else if (input[i] == '$' && i + 1 < len)
        {
            i += append_env_var(&res, &res_len, input + i + 1);
        }
        else
        {
            res[res_len++] = input[i];
        }
    }
    res[res_len] = '\0';
    return res;
}

/* Create a new AST node */
static ASTNode *create_ast_node(TokenType type, const char *value)
{
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->left = NULL;
    node->right = NULL;
    node->redirect_fd = -1;
    node->redirect_file = NULL;
    return node;
}

/* Report a syntax error */
static void syntax_error(const char *message)
{
    fprintf(stderr, "Syntax error: %s\n", message);
    if (isatty(STDIN_FILENO))
    {
        longjmp(parse_recovery, 1); /* Recover in interactive mode */
    }
    else
    {
        exit(EXIT_FAILURE);
    }
}

/* Consume the current token if it matches the expected type */
static void consume_token(TokenType type)
{
    if (current_token.type == type)
    {
        free_token(current_token);
        current_token = lexer_next_token();
    }
    else
    {
        syntax_error("Unexpected token");
    }
}

/* Parse a command */
static ASTNode *parse_command()
{
    if (current_token.type == TOKEN_COMMAND || current_token.type == TOKEN_QUOTE ||
        current_token.type == TOKEN_SUBSHELL)
    {
        ASTNode *node = create_ast_node(current_token.type, current_token.value);
        consume_token(current_token.type);
        while (current_token.type == TOKEN_COMMAND || current_token.type == TOKEN_QUOTE ||
               current_token.type == TOKEN_SUBSHELL)
        {
            ASTNode *arg_node = create_ast_node(current_token.type, current_token.value);
            ASTNode *temp = node;
            while (temp->right)
                temp = temp->right;
            temp->right = arg_node;
            consume_token(current_token.type);
        }
        return node;
    }
    syntax_error("Expected command");
    return NULL;
}

/* Forward declaration for parse_expression */
static ASTNode *parse_expression();

/* Parse parentheses */
static ASTNode *parse_parentheses()
{
    consume_token(TOKEN_LPAREN);
    ASTNode *node = parse_expression();
    consume_token(TOKEN_RPAREN);
    return node;
}

/* Parse a factor (command, parentheses with optional redirections) */
static ASTNode *parse_factor()
{
    ASTNode *node = NULL;
    if (current_token.type == TOKEN_COMMAND || current_token.type == TOKEN_QUOTE ||
        current_token.type == TOKEN_SUBSHELL)
    {
        node = parse_command();
    }
    else if (current_token.type == TOKEN_LPAREN)
    {
        node = parse_parentheses();
    }
    else
    {
        syntax_error("Expected factor");
        return NULL;
    }

    /* Parse redirections */
    while (current_token.type == TOKEN_REDIRECT_OUT ||
           current_token.type == TOKEN_REDIRECT_IN ||
           current_token.type == TOKEN_REDIRECT_APPEND ||
           current_token.type == TOKEN_REDIRECT_ERR)
    {
        TokenType redirect_type = current_token.type;
        int fd = (redirect_type == TOKEN_REDIRECT_ERR) ? 2 : 1;
        if (redirect_type == TOKEN_REDIRECT_IN)
            fd = 0;
        consume_token(redirect_type);
        if (current_token.type != TOKEN_COMMAND && current_token.type != TOKEN_QUOTE)
        {
            syntax_error("Expected filename after redirection");
        }
        node->redirect_fd = fd;
        node->redirect_file = strdup(current_token.value);
        consume_token(current_token.type);
    }

    return node;
}

/* Parse a term (factor with optional pipes) */
static ASTNode *parse_term()
{
    ASTNode *node = parse_factor();
    while (current_token.type == TOKEN_PIPE)
    {
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

/* Parse an expression (term with optional AND/OR/semicolon) */
static ASTNode *parse_expression()
{
    ASTNode *node = parse_term();
    while (current_token.type == TOKEN_AND || current_token.type == TOKEN_OR ||
           current_token.type == TOKEN_SEMICOLON)
    {
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

/* Parse the input and return the AST */
ASTNode *parse()
{
    current_token = lexer_next_token();
    if (current_token.type == TOKEN_EOF)
    {
        return NULL;
    }
    return parse_expression();
}

/* Free the memory allocated for the AST */
void free_ast(ASTNode *root)
{
    if (root)
    {
        free_ast(root->left);
        free_ast(root->right);
        free(root->value);
        free(root->redirect_file);
        free(root);
    }
}

/* Print a syntax error message */
void print_syntax_error(const char *message)
{
    fprintf(stderr, "Syntax error: %s\n", message);
}

/* Execute command substitution */
static char *execute_subshell(const char *cmd)
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        return strdup("");
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        parse_and_execute((char *)cmd);
        _exit(0);
    }
    else if (pid < 0)
    {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return strdup("");
    }
    else
    {
        close(pipefd[1]);
        char *result = malloc(4096);
        size_t len = 0;
        char buf[1024];
        ssize_t n;
        while ((n = read(pipefd[0], buf, sizeof(buf))) > 0)
        {
            result = realloc(result, len + n + 1);
            memcpy(result + len, buf, n);
            len += n;
        }
        result[len] = '\0';
        close(pipefd[0]);
        int status;
        waitpid(pid, &status, 0);
        /* Trim trailing newline */
        if (len > 0 && result[len - 1] == '\n')
            result[len - 1] = '\0';
        return result;
    }
}

/* Execute the AST */
int execute_ast(ASTNode *root)
{
    if (root == NULL)
    {
        return 0;
    }
    int left_status = 0;
    int right_status = 0;
    switch (root->type)
    {
        case TOKEN_COMMAND:
        case TOKEN_QUOTE:
        case TOKEN_SUBSHELL:
        {
            char *args[1024];
            int i = 0;
            ASTNode *temp = root;
            while (temp)
            {
                if (temp->type == TOKEN_SUBSHELL)
                {
                    args[i] = execute_subshell(temp->value);
                }
                else
                {
                    args[i] = expand_variables(temp->value, temp->type);
                }
                i++;
                temp = temp->right;
            }
            args[i] = NULL;

            int saved_fd = -1;
            if (root->redirect_file)
            {
                int flags = O_WRONLY | O_CREAT;
                if (root->redirect_fd == 0)
                {
                    flags = O_RDONLY;
                }
                else if (root->redirect_fd == 1 || root->redirect_fd == 2)
                {
                    flags |= (root->type == TOKEN_REDIRECT_APPEND) ? O_APPEND : O_TRUNC;
                }
                int fd = open(root->redirect_file, flags, 0644);
                if (fd == -1)
                {
                    perror(root->redirect_file);
                    return 1;
                }
                saved_fd = dup(root->redirect_fd);
                dup2(fd, root->redirect_fd);
                close(fd);
            }

            int status = exec_command(args[0], args);

            if (saved_fd != -1)
            {
                dup2(saved_fd, root->redirect_fd);
                close(saved_fd);
            }

            for (i = 0; args[i]; i++)
                free(args[i]);
            return status;
        }
        case TOKEN_PIPE:
        {
            int pipefd[2];
            if (pipe(pipefd) == -1)
            {
                perror("pipe");
                return 1;
            }
            pid_t pid = fork();
            if (pid == 0)
            {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
                int status = execute_ast(root->left);
                _exit(status);
            }
            else if (pid < 0)
            {
                perror("fork");
                return 1;
            }
            else
            {
                close(pipefd[1]);
                int saved_stdin = dup(STDIN_FILENO);
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[0]);
                right_status = execute_ast(root->right);
                dup2(saved_stdin, STDIN_FILENO);
                close(saved_stdin);
                waitpid(pid, &left_status, 0);
            }
            return right_status;
        }
        case TOKEN_AND:
            left_status = execute_ast(root->left);
            if (left_status == 0)
            {
                return execute_ast(root->right);
            }
            return left_status;
        case TOKEN_OR:
            left_status = execute_ast(root->left);
            if (left_status != 0)
            {
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
}

/* Execute a command */
int exec_command(char *cmd, char **args)
{
    if (is_builtin(cmd))
    {
        return run_builtin(args);
    }

    /* Perform globbing */
    glob_t glob_result;
    char **new_args = malloc(1024 * sizeof(char *));
    int new_argc = 0;
    for (int i = 0; args[i] && new_argc < 1023; i++)
    {
        if (strchr(args[i], '*') || strchr(args[i], '?') || strchr(args[i], '['))
        {
            if (glob(args[i], GLOB_NOCHECK, NULL, &glob_result) == 0)
            {
                for (size_t j = 0; glob_result.gl_pathv[j] && new_argc < 1023; j++)
                {
                    new_args[new_argc++] = strdup(glob_result.gl_pathv[j]);
                }
                globfree(&glob_result);
            }
            else
            {
                new_args[new_argc++] = strdup(args[i]);
            }
        }
        else
        {
            new_args[new_argc++] = strdup(args[i]);
        }
    }
    new_args[new_argc] = NULL;

    /* Check if cmd contains a path */
    if (strchr(cmd, '/'))
    {
        struct stat st;
        if (stat(cmd, &st) == 0)
        {
            if (!S_ISREG(st.st_mode) || !(st.st_mode & S_IXUSR))
            {
                fprintf(stderr, "%s: Not an executable file\n", cmd);
                for (int i = 0; new_args[i]; i++)
                    free(new_args[i]);
                free(new_args);
                return 1;
            }
        }
        else if (errno == ENOENT)
        {
            fprintf(stderr, "%s: No such file or directory\n", cmd);
            for (int i = 0; new_args[i]; i++)
                free(new_args[i]);
                free(new_args);
                return 1;
            }
            else
            {
                perror(cmd);
                for (int i = 0; new_args[i]; i++)
                    free(new_args[i]);
                free(new_args);
                return 1;
            }
    }
    else
    {
        /* Search PATH for the command */
        char *path_env = getenv("PATH");
        if (!path_env)
        {
            fprintf(stderr, "%s: command not found\n", cmd);
            for (int i = 0; new_args[i]; i++)
                free(new_args[i]);
            free(new_args);
            return 1;
        }
        char *path_copy = strdup(path_env);
        if (!path_copy)
        {
            perror("strdup");
            for (int i = 0; new_args[i]; i++)
                free(new_args[i]);
            free(new_args);
            return 1;
        }
        char full_path[MAX_PATH_LEN];
        char *dir = strtok(path_copy, ":");
        int found = 0;
        while (dir)
        {
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
            struct stat st;
            if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR))
            {
                found = 1;
                break;
            }
            dir = strtok(NULL, ":");
        }
        free(path_copy);
        if (!found)
        {
            fprintf(stderr, "%s: command not found\n", cmd);
            for (int i = 0; new_args[i]; i++)
                free(new_args[i]);
            free(new_args);
            return 1;
        }
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        execvp(cmd, new_args);
        perror(cmd);
        for (int i = 0; new_args[i]; i++)
            free(new_args[i]);
        free(new_args);
        _exit(1);
    }
    else if (pid < 0)
    {
        perror("fork");
        for (int i = 0; new_args[i]; i++)
            free(new_args[i]);
        free(new_args);
        return -1;
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        for (int i = 0; new_args[i]; i++)
            free(new_args[i]);
        free(new_args);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
}

/* Parse and execute the input line */
void parse_and_execute(char *line)
{
    lexer_init(line);
    if (setjmp(parse_recovery) == 0)
    {
        ASTNode *root = parse();
        if (root)
        {
            execute_ast(root);
            free_ast(root);
        }
    }
}
