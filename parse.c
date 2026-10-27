#include "parse.h"
#include "builtins.h"
#include "lexer.h"
#include "libtinyio/stdio.h"
#include "libtinyio/string.h"
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <setjmp.h>
#include <signal.h>

#define MAX_PATH_LEN 4096

#define MAX_BG_PROCS 100

extern int last_exit_status; /* Declare the global variable */
extern char *script_name; /* Declare script_name from shush.c */
extern char **script_args; /* Declare script_args from shush.c */
extern int script_argc; /* Declare script_argc from shush.c */
static Token current_token;
static jmp_buf parse_recovery;
static bool parse_exits; /* in a $(...) child: a syntax error ends it */
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

/* Append n bytes to the growing string *res (length *len, capacity *cap) */
static void res_add(char **res, size_t *len, size_t *cap, const char *s, size_t n)
{
    if (*len + n + 1 > *cap)
    {
        *cap = (*len + n + 1) * 2;
        *res = realloc(*res, *cap);
        if (!*res)
        {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
    }
    memcpy(*res + *len, s, n);
    *len += n;
    (*res)[*len] = '\0';
}

/*
 * Expand $name or ${name} at env_name (just after the '$'). Returns how
 * many characters it used. A lone '$' stays literal; the ${...} forms
 * other than ${name} are not supported yet: "bad substitution", and the
 * whole ${...} is skipped.
 */
static size_t append_env_var(char **res, size_t *res_len, size_t *cap, const char *env_name)
{
    bool braced = (*env_name == '{');
    const char *name = env_name + braced;
    const char *end = name;

    while (*end && (isalnum((unsigned char)*end) || *end == '_'))
        end++;
    size_t name_len = end - name;
    if (braced && (*end != '}' || name_len == 0))
    {
        fprintf(stderr, "shush: bad substitution\n");
        const char *close = strchr(env_name, '}');
        return close ? (size_t)(close - env_name) + 1 : strlen(env_name);
    }
    if (name_len == 0)
    {
        res_add(res, res_len, cap, "$", 1);
        return 0;
    }
    char var[name_len + 1];
    memcpy(var, name, name_len);
    var[name_len] = '\0';
    const char *val = getenv(var);
    if (val)
        res_add(res, res_len, cap, val, strlen(val));
    return name_len + (braced ? 2 : 0);
}

/* Function to expand variables */
char *expand_variables(const char *input, TokenType token_type)
{
    if (token_type == TOKEN_QUOTE) {
        return strdup(input); /* No expansion for single quotes */
    }

    size_t len = strlen(input);
    size_t res_len = 0, cap = 0;
    char *res = NULL;

    res_add(&res, &res_len, &cap, "", 0);
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '~') {
            /* Without HOME a ~ stays as it is */
            const char *home = getenv("HOME");
            res_add(&res, &res_len, &cap, home ? home : "~", strlen(home ? home : "~"));
        }
        else if (input[i] == '$' && i + 1 < len) {
            if (input[i + 1] == '?') {
                /* Handle $? expansion */
                char status_str[12];
                snprintf(status_str, sizeof(status_str), "%d", last_exit_status);
                res_add(&res, &res_len, &cap, status_str, strlen(status_str));
                i++; /* Skip the '?' */
            }
            else if (isdigit((unsigned char)input[i + 1]))
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
                    res_add(&res, &res_len, &cap, val, strlen(val));
            }
            else {
                i += append_env_var(&res, &res_len, &cap, input + i + 1);
            }
        }
        else {
            res_add(&res, &res_len, &cap, input + i, 1);
        }
    }
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
    node->redirections = NULL; /* Initialize new redirections field */
    node->background = false;
    return node;
}

/* True if the string is non-empty and every character is a digit */
static bool is_all_digits(const char *s)
{
    if (!s || !*s)
        return false;
    for (; *s; s++)
    {
        if (!isdigit((unsigned char)*s))
            return false;
    }
    return true;
}

/* Report a syntax error */
static void syntax_error(const char *message)
{
    fprintf(stderr, "Syntax error: %s\n", message);
    if (isatty(STDIN_FILENO) && !parse_exits)
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

/* Forward declaration for parse_grouping */
static ASTNode *parse_grouping();

/* Parse parentheses */
static ASTNode *parse_parentheses()
{
    consume_token(TOKEN_LPAREN);
    ASTNode *node = parse_expression();
    consume_token(TOKEN_RPAREN);
    return node;
}

/* Parse grouping */
static ASTNode *parse_grouping()
{
    consume_token(TOKEN_LBRACE);
    ASTNode *expression_node = parse_expression();
    consume_token(TOKEN_RBRACE);

    ASTNode *group_node = create_ast_node(TOKEN_GROUP, NULL);
    group_node->left = expression_node;

    return group_node;
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
    else if (current_token.type == TOKEN_LBRACE)
    {
        node = parse_grouping();
    }
    else
    {
        syntax_error("Expected factor");
        return NULL;
    }

    /* Parse redirections */
    while (current_token.type == TOKEN_IO_NUMBER ||
           current_token.type == TOKEN_REDIRECT_OUT ||
           current_token.type == TOKEN_REDIRECT_IN ||
           current_token.type == TOKEN_REDIRECT_APPEND ||
           current_token.type == TOKEN_HEREDOC ||
           current_token.type == TOKEN_HERESTRING ||
           current_token.type == TOKEN_REDIRECT_BOTH)
    {
        Redirection *redir = malloc(sizeof(Redirection));
        if (!redir) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        redir->next = NULL;
        redir->is_fd_dup = false;
        redir->dup_target_fd = -1;
        redir->file = NULL;

        int fd = -1;
        if (current_token.type == TOKEN_IO_NUMBER) {
            fd = atoi(current_token.value);
            consume_token(TOKEN_IO_NUMBER);
        }

        redir->type = current_token.type;
        if (fd == -1) { // Set default fd if not specified
            if (redir->type == TOKEN_REDIRECT_IN || redir->type == TOKEN_HEREDOC ||
                     redir->type == TOKEN_HERESTRING)
                fd = 0;
            else
                fd = 1; // Default to stdout for >, >>, &>
        }
        redir->fd = fd;

        consume_token(redir->type);

        /* N>&M / N<&M: duplicate fd M onto fd N instead of opening a file.
         * Not supported for &> (always a file target) or heredocs/herestrings
         * (always content). Closing forms (N>&-) are not supported. */
        if ((redir->type == TOKEN_REDIRECT_OUT || redir->type == TOKEN_REDIRECT_IN ||
             redir->type == TOKEN_REDIRECT_APPEND) &&
            current_token.type == TOKEN_AMPERSAND)
        {
            consume_token(TOKEN_AMPERSAND);
            /* The lexer tokenizes digits context-free: in "2>&1 > file" the
             * '1' is itself immediately followed by another '>', so it gets
             * classified as TOKEN_IO_NUMBER (meant for that later redirect),
             * not TOKEN_COMMAND. Accept either; only the digit value
             * matters here. */
            if ((current_token.type != TOKEN_COMMAND && current_token.type != TOKEN_IO_NUMBER) ||
                !is_all_digits(current_token.value))
            {
                syntax_error("Expected a file descriptor number after '&'");
            }
            redir->is_fd_dup = true;
            redir->dup_target_fd = atoi(current_token.value);
            consume_token(current_token.type);
        }
        else
        {
            if (current_token.type != TOKEN_COMMAND && current_token.type != TOKEN_QUOTE &&
                current_token.type != TOKEN_STRING)
            {
                syntax_error("Expected filename or string after redirection");
            }
            redir->file = strdup(current_token.value);
            consume_token(current_token.type);
        }

        // Add redirection to the linked list
        if (node->redirections == NULL) {
            node->redirections = redir;
        } else {
            Redirection *temp_redir = node->redirections;
            while (temp_redir->next) {
                temp_redir = temp_redir->next;
            }
            temp_redir->next = redir;
        }
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
    ASTNode *root = parse_expression();
    if (current_token.type != TOKEN_EOF)
    {
        /* Trailing input the grammar couldn't consume -- report it instead
         * of silently dropping it (and whatever comes after it). */
        syntax_error("Unexpected token");
    }
    return root;
}

/* Free the memory allocated for the AST */
void free_ast(ASTNode *root)
{
    if (root)
    {
        free_ast(root->left);
        free_ast(root->right);
        free(root->value);

        // Free redirections linked list
        Redirection *current = root->redirections;
        while (current) {
            Redirection *next = current->next;
            free(current->file);
            free(current);
            current = next;
        }
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
        parse_exits = true; /* not a second interactive shell on the tty */
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

static int execute_node(ASTNode *root);

/* Execute the AST; $? follows every command, not only every line */
int execute_ast(ASTNode *root)
{
    int status = execute_node(root);

    last_exit_status = status;
    return status;
}

static int execute_node(ASTNode *root)
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
            int n = 0;
            ASTNode *temp = root;

            for (temp = root; temp; temp = temp->right)
                n++;
            char **args = calloc(n + 1, sizeof(char *));
            if (!args)
            {
                perror("calloc");
                return 1;
            }
            int i = 0;
            temp = root;

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
                }
                args[i] = strdup(expanded);
                free(expanded);
                i++;
                temp = temp->right;
            }
            args[i] = NULL;

            // Handle redirections
            int saved_fds[3]; // For stdin, stdout, stderr
            bool fd_saved[3] = {false, false, false};
            Redirection *current_redir = root->redirections;
            while (current_redir) {
                int flags;
                int opened_fd = -1;

                if (current_redir->is_fd_dup) {
                    if (current_redir->fd >= 0 && current_redir->fd < 3) {
                        if (!fd_saved[current_redir->fd]) {
                            saved_fds[current_redir->fd] = dup(current_redir->fd);
                            fd_saved[current_redir->fd] = true;
                        }
                    }
                    if (dup2(current_redir->dup_target_fd, current_redir->fd) == -1) {
                        fprintf(stderr, "shush: %d: Bad file descriptor\n",
                                current_redir->dup_target_fd);
                        for (int j = 0; args[j]; j++)
                            free(args[j]);
                        free(args);
                        return 1;
                    }
                    current_redir = current_redir->next;
                    continue;
                }

                if (current_redir->type == TOKEN_HEREDOC) {
                    fprintf(stderr, "shush: here documents (<<) are not supported in scripts.\n");
                    for (int j = 0; args[j]; j++)
                        free(args[j]);
                    free(args);
                    return 1; /* Return error */
                } else if (current_redir->type == TOKEN_HERESTRING) {
                    char tmpfile[] = "/tmp/shush_herestring_XXXXXX";
                    opened_fd = mkstemp(tmpfile);
                    if (opened_fd == -1) {
                        perror("mkstemp");
                        for (int j = 0; args[j]; j++)
                            free(args[j]);
                        free(args);
                        return 1;
                    }
                    unlink(tmpfile);
                    char *expanded = expand_variables(current_redir->file, TOKEN_STRING);
                    write(opened_fd, expanded, strlen(expanded));
                    write(opened_fd, "\n", 1); /* bash appends a newline to <<< content */
                    free(expanded);
                    lseek(opened_fd, 0, SEEK_SET);
                } else if (current_redir->type == TOKEN_REDIRECT_BOTH) {
                    flags = O_WRONLY | O_CREAT | O_TRUNC;
                    opened_fd = open(current_redir->file, flags, 0644);
                    if (opened_fd == -1) {
                        perror(current_redir->file);
                        for (int j = 0; args[j]; j++)
                            free(args[j]);
                        free(args);
                        return 1;
                    }
                } else {
                    flags = O_WRONLY | O_CREAT;
                    if (current_redir->fd == 0) // Input redirection
                        flags = O_RDONLY;
                    else if (current_redir->fd == 1) // Output redirection (>, >>)
                        flags |= (current_redir->type == TOKEN_REDIRECT_APPEND) ? O_APPEND : O_TRUNC;
                    else if (current_redir->fd == 2) // Error redirection (2>)
                        flags |= O_TRUNC;

                    opened_fd = open(current_redir->file, flags, 0644);
                    if (opened_fd == -1) {
                        perror(current_redir->file);
                        for (int j = 0; args[j]; j++)
                            free(args[j]);
                        free(args);
                        return 1;
                    }
                }

                if (current_redir->fd >= 0 && current_redir->fd < 3) {
                    if (!fd_saved[current_redir->fd]) {
                        saved_fds[current_redir->fd] = dup(current_redir->fd);
                        fd_saved[current_redir->fd] = true;
                    }
                }

                if (current_redir->type == TOKEN_REDIRECT_BOTH) {
                    dup2(opened_fd, STDOUT_FILENO);
                    dup2(opened_fd, STDERR_FILENO);
                } else {
                    dup2(opened_fd, current_redir->fd);
                }
                close(opened_fd);
                current_redir = current_redir->next;
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

            // Restore saved file descriptors
            for (int j = 0; j < 3; j++) {
                if (fd_saved[j]) {
                    dup2(saved_fds[j], j);
                    close(saved_fds[j]);
                }
            }

            for (int j = 0; args[j]; j++)
                free(args[j]);
            free(args);
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
        case TOKEN_GROUP:
            return execute_ast(root->left);
        default:
            fprintf(stderr, "Unknown AST node type\n");
            return 1;
    }
}

/*
 * Match a single path component against a shell wildcard pattern
 * ('*', '?', '[...]'/'[!...]'/'[^...]' with a-z style ranges). No
 * backslash-escaping, matching the lexer, which doesn't support it either.
 * An unterminated '[' with no closing ']' is treated as a literal
 * character, matching glob(3)/fnmatch(3) behavior.
 */
static int glob_match(const char *pattern, const char *name)
{
    while (*pattern)
    {
        if (*pattern == '*')
        {
            while (*pattern == '*')
                pattern++;
            if (!*pattern)
                return 1;
            for (; *name; name++)
            {
                if (glob_match(pattern, name))
                    return 1;
            }
            return 0;
        }
        else if (*pattern == '?')
        {
            if (!*name)
                return 0;
            pattern++;
            name++;
        }
        else if (*pattern == '[')
        {
            const char *close = pattern + 1;
            if (*close == '!' || *close == '^')
                close++;
            if (*close == ']')
                close++;
            while (*close && *close != ']')
                close++;

            if (*close != ']')
            {
                /* No closing bracket: '[' is a literal character */
                if (*name != '[')
                    return 0;
                pattern++;
                name++;
                continue;
            }

            if (!*name)
                return 0;

            pattern++;
            int negate = 0;
            if (*pattern == '!' || *pattern == '^')
            {
                negate = 1;
                pattern++;
            }
            int matched = 0;
            int first = 1;
            while (*pattern && (*pattern != ']' || first))
            {
                first = 0;
                if (pattern[1] == '-' && pattern[2] && pattern[2] != ']')
                {
                    if ((unsigned char)*name >= (unsigned char)pattern[0] &&
                        (unsigned char)*name <= (unsigned char)pattern[2])
                        matched = 1;
                    pattern += 3;
                }
                else
                {
                    if (*name == *pattern)
                        matched = 1;
                    pattern++;
                }
            }
            pattern++; /* skip ']' */
            if (negate)
                matched = !matched;
            if (!matched)
                return 0;
            name++;
        }
        else
        {
            if (*name != *pattern)
                return 0;
            pattern++;
            name++;
        }
    }
    return *name == '\0';
}

static int glob_cmp(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

/*
 * Minimal shell glob: expands '*'/'?'/'[...]' against the final path
 * component only. A wildcard in an earlier path segment, before the last
 * slash, is not supported -- that segment is used literally, so such a
 * pattern just won't match (falls back to the literal-pattern behavior
 * below, same as glob(3)'s GLOB_NOCHECK). Replaces glob(3)/fnmatch(3)
 * entirely to avoid pulling in their brace/tilde/collating-symbol
 * machinery, which this shell never needs.
 *
 * Returns NULL (and *out_count == 0) if nothing matched, so the caller can
 * fall back to using the pattern literally.
 */
static char **shush_glob(const char *pattern, size_t *out_count)
{
    *out_count = 0;

    const char *slash = strrchr(pattern, '/');
    char *dir;
    const char *base_pattern;
    if (slash)
    {
        size_t dir_len = slash - pattern;
        dir = malloc(dir_len + 1);
        if (!dir)
            return NULL;
        memcpy(dir, pattern, dir_len);
        dir[dir_len] = '\0';
        base_pattern = slash + 1;
    }
    else
    {
        dir = strdup(".");
        if (!dir)
            return NULL;
        base_pattern = pattern;
    }

    DIR *dp = opendir(dir[0] ? dir : "/");
    if (!dp)
    {
        free(dir);
        return NULL;
    }

    int hide_dot = (base_pattern[0] != '.');
    size_t cap = 32;
    char **matches = malloc(cap * sizeof(char *));
    if (!matches)
    {
        closedir(dp);
        free(dir);
        return NULL;
    }
    size_t count = 0;

    struct dirent *entry;
    while ((entry = readdir(dp)))
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (hide_dot && entry->d_name[0] == '.')
            continue;
        if (!glob_match(base_pattern, entry->d_name))
            continue;

        if (count == cap)
        {
            cap *= 2;
            char **grown = realloc(matches, cap * sizeof(char *));
            if (!grown)
                break;
            matches = grown;
        }

        char *full;
        if (slash)
        {
            size_t len = strlen(dir) + 1 + strlen(entry->d_name) + 1;
            full = malloc(len);
            if (full)
                snprintf(full, len, "%s/%s", dir, entry->d_name);
        }
        else
        {
            full = strdup(entry->d_name);
        }
        if (full)
            matches[count++] = full;
    }
    closedir(dp);
    free(dir);

    if (count == 0)
    {
        free(matches);
        return NULL;
    }

    qsort(matches, count, sizeof(char *), glob_cmp);
    *out_count = count;
    return matches;
}

/* Append one argument to a growing argv (count *n, capacity *cap) */
static void argv_add(char ***v, int *n, int *cap, char *arg)
{
    if (*n + 2 > *cap)
    {
        *cap = (*n + 2) * 2;
        *v = realloc(*v, *cap * sizeof(char *));
        if (!*v)
        {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
    }
    (*v)[(*n)++] = arg;
    (*v)[*n] = NULL;
}

static void argv_free(char **v)
{
    for (int i = 0; v[i]; i++)
        free(v[i]);
    free(v);
}

/*
 * Execute a command. The status is the command's, or as POSIX has it:
 * 127 when it is not found, 126 when it cannot be executed, 128+n when
 * a signal n ended it.
 */
int exec_command(char *cmd, char **args)
{
    if (is_builtin(cmd))
    {
        return run_builtin(args);
    }

    /* Perform globbing */
    char **new_args = NULL;
    int new_argc = 0, cap = 0;
    argv_add(&new_args, &new_argc, &cap, NULL);
    new_argc = 0;
    for (int i = 0; args[i]; i++)
    {
        if (strchr(args[i], '*') || strchr(args[i], '?') || strchr(args[i], '['))
        {
            size_t glob_count = 0;
            char **glob_matches = shush_glob(args[i], &glob_count);
            if (glob_matches)
            {
                for (size_t j = 0; j < glob_count; j++)
                    argv_add(&new_args, &new_argc, &cap, glob_matches[j]);
                free(glob_matches);
                continue;
            }
        }
        argv_add(&new_args, &new_argc, &cap, strdup(args[i]));
    }

    /* Check if cmd contains a path */
    if (strchr(cmd, '/'))
    {
        struct stat st;
        if (stat(cmd, &st) == 0)
        {
            if (!S_ISREG(st.st_mode) || !(st.st_mode & S_IXUSR))
            {
                fprintf(stderr, "%s: Permission denied\n", cmd);
                argv_free(new_args);
                return 126;
            }
        }
        else
        {
            perror(cmd);
            argv_free(new_args);
            return errno == ENOENT || errno == ENOTDIR ? 127 : 126;
        }
    }
    else
    {
        /* Search PATH for the command */
        char *path_env = getenv("PATH");
        int found = 0;
        if (path_env)
        {
            char *path_copy = strdup(path_env);
            if (!path_copy)
            {
                perror("strdup");
                argv_free(new_args);
                return 1;
            }
            char full_path[MAX_PATH_LEN];
            for (char *dir = strtok(path_copy, ":"); dir; dir = strtok(NULL, ":"))
            {
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
                struct stat st;
                if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR))
                {
                    found = 1;
                    break;
                }
            }
            free(path_copy);
        }
        if (!found)
        {
            fprintf(stderr, "%s: command not found\n", cmd);
            argv_free(new_args);
            return 127;
        }
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        execvp(cmd, new_args);
        perror(cmd);
        _exit(errno == ENOENT ? 127 : 126);
    }
    else if (pid < 0)
    {
        perror("fork");
        argv_free(new_args);
        return 1;
    }
    int status;
    waitpid(pid, &status, 0);
    argv_free(new_args);
    return WIFEXITED(status) ? WEXITSTATUS(status) :
           WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1;
}

static void parse_and_execute_recursive(char *line, char *last_expanded_alias);

/* Main entry point for parsing and execution */
void parse_and_execute(char *line) {
    parse_and_execute_recursive(line, NULL);
}

/* Internal recursive function to handle alias expansion and execution */
static void parse_and_execute_recursive(char *line, char *last_expanded_alias) {
    static char *expansion_stack[20];
    static int stack_top = -1;

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

    /* Add to history only once at the top level */
    if (last_expanded_alias == NULL) {
        add_to_history(line);
    }

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

    const char *alias_value = NULL;
    /* Prevent re-expansion for self-referential aliases like ls='ls -F' */
    if (last_expanded_alias && strcmp(first_word, last_expanded_alias) == 0) {
        // Do nothing, alias_value remains NULL
    } else {
        alias_value = lookup_alias(first_word);
    }

    if (alias_value) {
        /* Check for indirect loops */
        for (int i = 0; i <= stack_top; i++) {
            if (strcmp(expansion_stack[i], first_word) == 0) {
                fprintf(stderr, "shush: alias loop detected\n");
                last_exit_status = 1;
                free(first_word);
                return;
            }
        }

        if (stack_top >= 19) { /* 20 - 1 */
            fprintf(stderr, "shush: alias recursion limit reached\n");
            last_exit_status = 1;
            free(first_word);
            return;
        }

        /* Push the current alias onto the expansion stack */
        stack_top++;
        expansion_stack[stack_top] = first_word;

        /* Construct the new line with the expanded alias */
        size_t new_line_len = strlen(alias_value) + (rest ? strlen(rest) + 1 : 0) + 1;
        char *new_line = malloc(new_line_len);
        if (!new_line) {
            perror("malloc");
            last_exit_status = 1;
            stack_top--; /* Pop before returning */
            free(first_word);
            return;
        }
        if (rest) {
            snprintf(new_line, new_line_len, "%s %s", alias_value, rest);
        } else {
            strcpy(new_line, alias_value);
        }

        /* Recurse with the new line, passing the expanded alias name */
        parse_and_execute_recursive(new_line, first_word);

        /* Pop from the stack and clean up */
        stack_top--;
        free(new_line);
        free(first_word);
        return;
    }

    /* No alias found or expansion skipped, proceed to execution */
    free(first_word);

    lexer_init(line);
    if (setjmp(parse_recovery) != 0) {
        last_exit_status = 1;
        return;
    }

    ASTNode *root = parse();
    if (root) {
        last_exit_status = execute_ast(root);
        free_ast(root);
    } else {
        last_exit_status = 0;
    }
}
