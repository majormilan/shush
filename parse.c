/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Parsing and executing commands for Simple Humane Shell (shush).
 */

#include "parse.h"
#include "builtins.h"
#include "libtinyio/stdio.h"
#include "libtinyio/string.h"
#include <ctype.h>
#include "libtinyio/signal.h"
#include <stdbool.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAX_LINE 1024

static bool debug = false;

/* Function Prototypes */
static int exec_cmd(char *cmd);
static char *trim(char *str);
static char *parse_arg(char **cmd);
static void handle_chain(char *line);
static void handle_result(char sep, int status, bool *exec_next, char next_sep);
static size_t append_env_var(char **res, size_t *res_len, const char *env_name);
static int exec_external(char *args[]);
char *expand_variables(const char *input);
static void handle_redirection(char **args);
static char *parse_subshell(char **cmd);

void set_debug(bool mode) { debug = mode; }

void parse_and_execute(char *line) { handle_chain(line); }

static void handle_chain(char *line)
{
    char *cmd, *end;
    int pipefd[2], status = 0;
    int stdin_backup = dup(STDIN_FILENO);
    int stdout_backup = dup(STDOUT_FILENO);
    bool exec_next = true;
    char current_sep = 0, next_sep = 0;

    while (*line)
    {
        line = trim(line);
        if (!*line)
            break;

        /* Find the end of the current command */
        end = line;
        while (*end && *end != '|' && *end != '&' && *end != ';')
        {
            if (*end == '(')
            {
                /* Skip over the subshell */
                char *subshell_end = end;
                int depth = 1;
                while (*subshell_end && depth > 0)
                {
                    subshell_end++;
                    if (*subshell_end == '(')
                        depth++;
                    if (*subshell_end == ')')
                        depth--;
                }
                end = subshell_end;
            }
            end++;
        }

        /* Determine the next separator */
        current_sep = *end;
        if (current_sep == '&' && *(end + 1) == '&')
        {
            next_sep = '&';
            end++;
        }
        else if (current_sep == '|' && *(end + 1) == '|')
        {
            next_sep = '|';
            end++;
        }
        else
        {
            next_sep = 0;
        }

        cmd = strndup(line, end - line);
        if (!cmd)
        {
            perror("strndup");
            exit(1);
        }

        if (exec_next)
        {
            if (current_sep == '|')
            {
                /* Setup pipe for piping */
                if (pipe(pipefd) < 0)
                {
                    perror("pipe");
                    exit(1);
                }

                pid_t pid = fork();
                if (pid == 0)
                {
                    /* Child: Write to the pipe */
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[0]); /* Close unused read end */
                    close(pipefd[1]); /* Close write end after dup2 */
                    exec_cmd(cmd);
                    exit(1); /* If exec fails */
                }
                else if (pid < 0)
                {
                    perror("fork");
                    exit(1);
                }

                /* Parent: Read from the pipe */
                waitpid(pid, &status, 0);
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[1]); /* Close unused write end */
                close(pipefd[0]); /* Close read end after dup2 */
            }
            else
            {
                status = exec_cmd(cmd);
            }
        }

        handle_result(current_sep, status, &exec_next, next_sep);

        free(cmd);
        line = *end ? end + 1 : end;
    }

    /* Restore the original file descriptors */
    dup2(stdin_backup, STDIN_FILENO);
    dup2(stdout_backup, STDOUT_FILENO);
    close(stdin_backup);
    close(stdout_backup);
}

static void handle_result(char sep, int status, bool *exec_next, char next_sep)
{
    switch (sep)
    {
        case '&':
            if (next_sep == '&')
            {
                *exec_next = (status == 0);
            }
            else
            {
                *exec_next = true;
            }
            break;
        case '|':
            if (next_sep == '|')
            {
                *exec_next = (status != 0);
            }
            else
            {
                *exec_next = true;
            }
            break;
        case ';':
        default:
            *exec_next = true;
            break;
    }
}

static int exec_cmd(char *cmd)
{
    char *args[MAX_LINE / 2 + 1];
    int i = 0;

    while (*cmd)
    {
        cmd = trim(cmd);
        if (!*cmd)
            break;

        args[i++] = expand_variables(parse_arg(&cmd));
        if (!args[i - 1])
        {
            fprintf(stderr, "Failed to parse argument\n");
            return -1;
        }
    }
    args[i] = NULL;

    if (!args[0])
        return 0;

    if (debug)
    {
        printf("Executing command: %s\n", args[0]);
        for (int j = 0; j < i; j++)
            printf("arg[%d]: %s\n", j, args[j]);
    }

    if (!strcmp(args[0], "exit"))
        exit(0);

    add_to_history(args[0]);

    handle_redirection(args);

    if (is_builtin(args[0]))
    {
        run_builtin(args);
        return last_exit_status;
    }

    return exec_external(args);
}

static int exec_external(char *args[])
{
    pid_t pid = fork();

    if (pid == 0)
    {
        execvp(args[0], args);
        perror("shush");
        exit(1);
    }
    else if (pid < 0)
    {
        perror("shush: fork failed");
        return -1;
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
}

static char *trim(char *str)
{
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    end[1] = '\0';
    return str;
}

static char *parse_arg(char **cmd)
{
    char *str = *cmd, *start = str;
    bool quoted = false;

    while (*str && (quoted || !isspace((unsigned char)*str)))
    {
        if (*str == '"')
            quoted = !quoted;
        else if (*str == '\\' && *(str + 1))
            str++;
        str++;
    }

    char *tok = strndup(start, str - start);
    if (!tok)
    {
        perror("strndup");
        exit(1);
    }

    *cmd = *str ? str + 1 : str;
    return tok;
}

char *expand_variables(const char *input)
{
    size_t len = strlen(input);
    char *res = malloc(len + 1);
    if (!res)
    {
        perror("malloc");
        exit(1);
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
                exit(1);
            }
            size_t home_len = strlen(home);
            res = realloc(res, res_len + home_len + 1);
            if (!res)
            {
                perror("realloc");
                exit(1);
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
                exit(1);
            }
            strcpy(*res + *res_len, val);
            *res_len += val_len;
        }
    }
    return name_len;
}

static void handle_redirection(char **args)
{
    for (int i = 0; args[i]; i++)
    {
        if (strcmp(args[i], "<") == 0)
        {
            freopen(args[i + 1], "r", stdin);
            args[i] = NULL;
            break;
        }
        else if (strcmp(args[i], ">") == 0)
        {
            freopen(args[i + 1], "w", stdout);
            args[i] = NULL;
            break;
        }
        else if (strcmp(args[i], ">>") == 0)
        {
            freopen(args[i + 1], "a", stdout);
            args[i] = NULL;
            break;
        }
    }
}

static char *parse_subshell(char **cmd)
{
    char *str = *cmd + 1; /* Skip '(' */
    size_t depth = 1;

    char *start = str;
    while (*str && depth > 0)
    {
        if (*str == '(')
        {
            depth++;
        }
        else if (*str == ')')
        {
            depth--;
        }
        str++;
    }

    if (depth != 0)
    {
        fprintf(stderr, "Unmatched parentheses\n");
        exit(1);
    }

    char *subshell_cmd = strndup(start, str - start - 1);
    if (!subshell_cmd)
    {
        perror("strndup");
        exit(1);
    }

    *cmd = str;
    return subshell_cmd;
}
