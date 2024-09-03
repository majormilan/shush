/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Parsing and executing commands for Simple Humane Shell (shush).
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

#include "parse.h"
#include "builtins.h"

#define MAX_LINE 1024

static bool debug_mode = false;

static int execute_command(char *cmd);
static char *trim_whitespace(char *str);
static char *parse_quoted_string(char **cmd);
static void handle_command_chain(char *line);

void set_debug_mode(bool mode) {
    debug_mode = mode;
}

void parse_and_execute(char *line) {
    handle_command_chain(line);
}

static void handle_command_chain(char *line) {
    char *cmd_start = line;
    char *cmd_end;
    int status = 0;
    bool execute_next = true;

    while (*cmd_start) {
        cmd_start = trim_whitespace(cmd_start);
        if (!*cmd_start)
            break;

        cmd_end = cmd_start + strcspn(cmd_start, ";&|");

        if (execute_next) {
            char *command = strndup(cmd_start, cmd_end - cmd_start);
            char *expanded_command = expand_variables(command);
            free(command);

            status = execute_command(expanded_command);
            free(expanded_command);
        }

        cmd_start = cmd_end + 1;

        if (*cmd_end == '&') {
            execute_next = (status == 0);
            if (*(cmd_end + 1) == '&')
                cmd_start++;
        } else if (*cmd_end == '|') {
            execute_next = (status != 0);
            if (*(cmd_end + 1) == '|')
                cmd_start++;
        } else {
            execute_next = true;
        }
    }
}

static int execute_command(char *cmd) {
    char *args[MAX_LINE / 2 + 1];
    char *arg = cmd;
    int i = 0;

    while (*arg) {
        arg = trim_whitespace(arg);
        if (!*arg)
            break;

        args[i++] = parse_quoted_string(&arg);
    }
    args[i] = NULL;

    if (!args[0])
        return 0;

    if (debug_mode) {
        printf("Executing command: %s\n", args[0]);
        for (int j = 0; j < i; j++) {
            printf("arg[%d]: %s\n", j, args[j]);
        }
    }

    if (strcmp(args[0], "exit") == 0)
        exit(0);

    add_to_history(args[0]);

    if (is_builtin(args[0])) {
        run_builtin(args);
        return last_exit_status;
    }

    pid_t pid = fork();
    if (pid == 0) {
        execvp(args[0], args);
        perror("shush");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("shush: fork failed");
        return -1;
    } else {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
}

static char *trim_whitespace(char *str) {
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

static char *parse_quoted_string(char **cmd) {
    char *str = *cmd;
    char *start = str;
    bool in_quotes = false;

    while (*str && (in_quotes || !isspace((unsigned char)*str))) {
        if (*str == '"')
            in_quotes = !in_quotes;
        else if (*str == '\\' && *(str + 1) != '\0')
            str++;
        str++;
    }

    char *token = strndup(start, str - start);
    *cmd = *str ? str + 1 : str;
    return token;
}

char *expand_variables(const char *input) {
    if (!home_directory) {
        fprintf(stderr, "Error: HOME is not set\n");
        exit(EXIT_FAILURE);
    }

    size_t len = strlen(input);
    char *expanded = malloc(len + 1);
    if (!expanded) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    strcpy(expanded, input);

    char *result = malloc(len + 1);
    if (!result) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    size_t result_len = 0;

    for (size_t i = 0; i < len; i++) {
        if (expanded[i] == '~') {
            size_t home_len = strlen(home_directory);
            result = realloc(result, result_len + home_len + 1);
            if (!result) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            strcpy(result + result_len, home_directory);
            result_len += home_len;
        } else if (expanded[i] == '$' && i + 1 < len) {
            char *env_var_name = expanded + i + 1;
            char *end = env_var_name;

            while (*end && (isalnum(*end) || *end == '_'))
                end++;

            size_t var_name_len = end - env_var_name;
            if (var_name_len > 0) {
                char var_name[var_name_len + 1];
                strncpy(var_name, env_var_name, var_name_len);
                var_name[var_name_len] = '\0';

                char *var_value = getenv(var_name);
                if (var_value) {
                    size_t value_len = strlen(var_value);
                    result = realloc(result, result_len + value_len + 1);
                    if (!result) {
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }
                    strcpy(result + result_len, var_value);
                    result_len += value_len;
                }
                i += var_name_len;
            }
        } else {
            result = realloc(result, result_len + 2);
            if (!result) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            result[result_len++] = expanded[i];
        }
    }

    result[result_len] = '\0';
    free(expanded);
    return result;
}
