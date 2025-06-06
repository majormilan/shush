#include "parse.h"
#include "builtins.h"
#include "lexer.h"
#include "libtinyio/stdio.h"
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <glob.h>
#include <setjmp.h>
#include <signal.h>

#define MAX_PATH_LEN 4096
#define MAX_ALIAS_DEPTH 1
#define MAX_BG_PROCS 100

extern int last_exit_status; /* Declare the global variable */
extern char *script_name; /* Declare script_name from shush.c */
extern char **script_args; /* Declare script_args from shush.c */
extern int script_argc; /* Declare script_argc from shush.c */
static Token current_token;
static jmp_buf parse_recovery;
pid_t bg_procs[MAX_BG_PROCS];
int bg_proc_count = 0;

/* Clean up completed background processes */
static void cleanup_bg_procs()
{
    for (int i = 0; i < bg_proc_count; i++)
    {
        int status;
        pid_t result = waitpid(bg_procs[i], &status, WNOHANG);
        if (result > 0)
        {
            printf("[%d] Done\n", bg_procs[i]);
            memmove(&bg_procs[i], &bg_procs[i + 1], (bg_proc_count - i - 1) * sizeof(pid_t));
            bg_proc_count--;
            i--; /* Re-check the current index after memmove */
        }
    }
}

/* Utility function for variable expansion */
static size_t append_env_var(char **res, size_t *res_len, const char *env_name)
{
    const char *end = env_name;
    bool braced = (*end == '{');
    if (braced)
        end++;
    while (*end && (isalnum(*end) || *end == '_'))
        end++;
    if (braced && *end == '}')
        end++;
    size_t name_len = end - env_name - (braced ? 2 : 0);
    if (name_len)
    {
        char var[name_len + 1];
        strncpy(var, braced ? env_name + 1 : env_name, name_len);
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
    return braced ? name_len + 2 : name_len;
}

/* Function to expand variables */
char *expand_variables(const char *input, TokenType token_type)
{
    if (token_type == TOKEN_QUOTE) {
        return strdup(input); /* No expansion for single quotes */
    }

    size_t len = strlen(input);
    char *res = malloc(len + 32); /* Extra space for expansions */
    if (!res) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    size_t res_len = 0;

    for (size_t i = 0; i < len; i++) {
        if (input[i] == '~') {
            const char *home = getenv("HOME");
            if (!home) {
                fprintf(stderr, "Error: HOME not set\n");
                exit(EXIT_FAILURE);
            }
            size_t home_len = strlen(home);
            res = realloc(res, res_len + home_len + 1);
            if (!res) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            strcpy(res + res_len, home);
            res_len += home_len;
        }
        else if (input[i] == '$' && i + 1 < len) {
            if (input[i + 1] == '?') {
                /* Handle $? expansion */
                char status_str[12];
                snprintf(status_str, sizeof(status_str), "%d", last_exit_status);
                size_t status_len = strlen(status_str);
                res = realloc(res, res_len + status_len + 1);
                if (!res) {
                    perror("realloc");
                    exit(EXIT_FAILURE);
                }
                strcpy(res + res_len, status_str);
                res_len += status_len;
                i++; /* Skip the '?' */
            }
            else if (isdigit(input[i + 1]))
            {
                /* Handle $0, $1, $2, etc. */
                char num = input[i + 1];
                i++; /* Skip the digit */
                const char *val = NULL;
                if (num == '0' && script_name)
                {
                    val = script_name;
                }
                else if (num - '0' <= script_argc && script_args && num != '0')
                {
                    val = script_args[num - '0' - 1];
                }
                if (val)
                {
                    size_t val_len = strlen(val);
                    res = realloc(res, res_len + val_len + 1);
                    if (!res)
                    {
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }
                    strcpy(res + res_len, val);
                    res_len += val_len;
                }
            }
            else {
                i += append_env_var(&res, &res_len, input + i + 1);
            }
        }
        else {
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
    node->background = false;
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
        current_token.type == TOKEN_SUBSHELL || current_token.type == TOKEN_STRING)
    {
        ASTNode *node = create_ast_node(current_token.type, current_token.value);
        consume_token(current_token.type);
        while (current_token.type == TOKEN_COMMAND || current_token.type == TOKEN_QUOTE ||
               current_token.type == TOKEN_SUBSHELL || current_token.type == TOKEN_STRING)
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
        current_token.type == TOKEN_SUBSHELL || current_token.type == TOKEN_STRING)
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
           current_token.type == TOKEN_REDIRECT_ERR ||
           current_token.type == TOKEN_HEREDOC ||
           current_token.type == TOKEN_HERESTRING ||
           current_token.type == TOKEN_REDIRECT_BOTH)
    {
        TokenType redirect_type = current_token.type;
        int fd = 1; /* Default to stdout */
        if (redirect_type == TOKEN_REDIRECT_ERR)
            fd = 2;
        else if (redirect_type == TOKEN_REDIRECT_IN || redirect_type == TOKEN_HEREDOC ||
                 redirect_type == TOKEN_HERESTRING)
            fd = 0;
        else if (redirect_type == TOKEN_REDIRECT_OUT && isdigit(current_token.value[0]))
        {
            fd = atoi(current_token.value); /* Parse numeric file descriptor */
        }
        consume_token(redirect_type);
        if (current_token.type != TOKEN_COMMAND && current_token.type != TOKEN_QUOTE &&
            current_token.type != TOKEN_STRING)
        {
            syntax_error("Expected filename or string after redirection");
        }
        node->redirect_fd = fd;
        node->redirect_file = strdup(current_token.value);
        if (redirect_type == TOKEN_HEREDOC || redirect_type == TOKEN_HERESTRING)
            node->type = redirect_type;
        else if (redirect_type == TOKEN_REDIRECT_BOTH)
            node->type = TOKEN_REDIRECT_BOTH;
        consume_token(current_token.type);
    }

    /* Handle background process */
    if (current_token.type == TOKEN_AMPERSAND)
    {
        node->background = true;
        consume_token(TOKEN_AMPERSAND);
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
        lexer_init(cmd);
        ASTNode *root = parse();
        if (root)
        {
            int status = execute_ast(root);
            free_ast(root);
            _exit(status);
        }
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

/* Trim trailing spaces from a string */
static void trim_trailing_spaces(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && isspace(str[len - 1]))
    {
        str[len - 1] = '\0';
        len--;
    }
}

/* Execute the AST */
int execute_ast(ASTNode *root)
{
    if (root == NULL)
    {
        return 0;
    }

    cleanup_bg_procs();

    int left_status = 0;
    int right_status = 0;
    switch (root->type)
    {
        case TOKEN_COMMAND:
        case TOKEN_QUOTE:
        case TOKEN_SUBSHELL:
        case TOKEN_STRING:
        case TOKEN_HEREDOC:
        case TOKEN_HERESTRING:
        case TOKEN_REDIRECT_BOTH:
        {
            char *args[1024] = {NULL};
            int i = 0;
            ASTNode *temp = root;

            while (temp)
            {
                char *expanded;
                if (temp->type == TOKEN_SUBSHELL)
                {
                    expanded = execute_subshell(temp->value);
                }
                else
                {
                    expanded = expand_variables(temp->value, temp->type);
                    if (temp->type == TOKEN_STRING || temp->type == TOKEN_QUOTE)
                    {
                        trim_trailing_spaces(expanded);
                    }
                }
                args[i] = strdup(expanded);
                free(expanded);
                i++;
                temp = temp->right;
            }
            args[i] = NULL;

            int saved_fd = -1;
            int saved_fd2 = -1; /* For &> */
            int heredoc_fd = -1;
            if (root->redirect_file)
            {
                int flags;
                if (root->type == TOKEN_HEREDOC)
                {
                    char tmpfile[] = "/tmp/shush_heredoc_XXXXXX";
                    heredoc_fd = mkstemp(tmpfile);
                    if (heredoc_fd == -1)
                    {
                        perror("mkstemp");
                        for (int j = 0; args[j]; j++)
                            free(args[j]);
                        return 1;
                    }
                    unlink(tmpfile);
                    char *line = NULL;
                    size_t len = 0;
                    while (getline(&line, &len, stdin) != -1)
                    {
                        line[strcspn(line, "\n")] = '\0';
                        if (strcmp(line, root->redirect_file) == 0)
                            break;
                        write(heredoc_fd, line, strlen(line));
                        write(heredoc_fd, "\n", 1);
                    }
                    free(line);
                    lseek(heredoc_fd, 0, SEEK_SET);
                    saved_fd = dup(STDIN_FILENO);
                    dup2(heredoc_fd, STDIN_FILENO);
                    close(heredoc_fd);
                }
                else if (root->type == TOKEN_HERESTRING)
                {
                    char tmpfile[] = "/tmp/shush_herestring_XXXXXX";
                    heredoc_fd = mkstemp(tmpfile);
                    if (heredoc_fd == -1)
                    {
                        perror("mkstemp");
                        for (int j = 0; args[j]; j++)
                            free(args[j]);
                        return 1;
                    }
                    unlink(tmpfile);
                    char *expanded = expand_variables(root->redirect_file, TOKEN_STRING);
                    write(heredoc_fd, expanded, strlen(expanded));
                    free(expanded);
                    lseek(heredoc_fd, 0, SEEK_SET);
                    saved_fd = dup(STDIN_FILENO);
                    dup2(heredoc_fd, STDIN_FILENO);
                    close(heredoc_fd);
                }
                else if (root->type == TOKEN_REDIRECT_BOTH)
                {
                    flags = O_WRONLY | O_CREAT | O_TRUNC;
                    int fd = open(root->redirect_file, flags, 0644);
                    if (fd == -1)
                    {
                        perror(root->redirect_file);
                        for (int j = 0; args[j]; j++)
                            free(args[j]);
                        return 1;
                    }
                    saved_fd = dup(STDOUT_FILENO);
                    saved_fd2 = dup(STDERR_FILENO);
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }
                else
                {
                    flags = O_WRONLY | O_CREAT;
                    if (root->redirect_fd == 0)
                        flags = O_RDONLY;
                    else if (root->redirect_fd == 1)
                        flags |= (root->type == TOKEN_REDIRECT_APPEND) ? O_APPEND : O_TRUNC;
                    else if (root->redirect_fd == 2)
                        flags |= O_APPEND;
                    int fd = open(root->redirect_file, flags, 0644);
                    if (fd == -1)
                    {
                        perror(root->redirect_file);
                        for (int j = 0; args[j]; j++)
                            free(args[j]);
                        return 1;
                    }
                    saved_fd = dup(root->redirect_fd);
                    dup2(fd, root->redirect_fd);
                    close(fd);
                }
            }

            int status;
            if (root->background)
            {
                if (bg_proc_count >= MAX_BG_PROCS)
                {
                    fprintf(stderr, "shush: too many background processes\n");
                    status = 1;
                }
                else
                {
                    pid_t pid = fork();
                    if (pid == 0)
                    {
                        signal(SIGINT, SIG_IGN);
                        status = exec_command(args[0], args);
                        _exit(status);
                    }
                    else if (pid < 0)
                    {
                        perror("fork");
                        status = 1;
                    }
                    else
                    {
                        bg_procs[bg_proc_count++] = pid;
                        printf("[%d] %d\n", bg_proc_count, pid);
                        status = 0;
                    }
                }
            }
            else
            {
                status = exec_command(args[0], args);
            }

            if (saved_fd != -1)
            {
                dup2(saved_fd, root->redirect_fd);
                close(saved_fd);
            }
            if (saved_fd2 != -1)
            {
                dup2(saved_fd2, STDERR_FILENO);
                close(saved_fd2);
            }

            for (int j = 0; args[j]; j++)
                free(args[j]);
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
void parse_and_execute(char *line) {
    static int alias_depth = 0;

    if (alias_depth >= MAX_ALIAS_DEPTH) {
        fprintf(stderr, "shush: alias recursion limit reached\n");
        last_exit_status = 1;
        return;
    }

    /* Trim leading/trailing whitespace */
    while (isspace((unsigned char)*line)) line++;
    char *trimmed = line + strlen(line) - 1;
    while (trimmed > line && isspace((unsigned char)*trimmed)) *trimmed-- = '\0';

    if (!*line) {
        last_exit_status = 0;
        return;
    }

    /* Skip comment lines starting with '#' */
    if (*line == '#') {
        last_exit_status = 0;
        return;
    }

    /* Add to history before alias expansion (non-comment lines only) */
    add_to_history(line);

    /* Extract the first word (command) */
    char *first_word = strdup(line);
    if (!first_word) {
        perror("strdup");
        last_exit_status = 1;
        return;
    }

    char *space = strchr(first_word, ' ');
    char *rest = NULL;
    if (space) {
        *space = '\0';
        rest = line + (space - first_word) + 1;
    }

    const char *alias_value = lookup_alias(first_word);
    if (alias_value) {
        size_t new_line_len = strlen(alias_value) + (rest ? strlen(rest) + 1 : 0) + 1;
        char *new_line = malloc(new_line_len);
        if (!new_line) {
            perror("malloc");
            free(first_word);
            last_exit_status = 1;
            return;
        }
        if (rest) {
            snprintf(new_line, new_line_len, "%s %s", alias_value, rest);
        }
        else {
            strcpy(new_line, alias_value);
        }
        free(first_word);
        alias_depth++;
        parse_and_execute(new_line);
        alias_depth--;
        free(new_line);
        return;
    }

    free(first_word);

    lexer_init(line);
    if (setjmp(parse_recovery) != 0) {
        last_exit_status = 1;
        return;
    }

    ASTNode *root = parse();
    if (root)
    {
        last_exit_status = execute_ast(root);
        free_ast(root);
    }
    else {
        last_exit_status = 0;
    }
}
