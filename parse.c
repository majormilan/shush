#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <sys/wait.h>
#include "parse.h"
#include "builtins.h"

#define MAX_LINE 80

// Utility function to check if a path is a directory
bool is_directory(const char *path) {
    struct stat path_stat;
    return (stat(path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode));
}

// Execute a single command
static int execute_command(char *cmd) {
    char *args[MAX_LINE / 2 + 1];
    char *token = strtok(cmd, " \t\r\n\a");
    int i = 0;

    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " \t\r\n\a");
    }
    args[i] = NULL;

    if (args[0] == NULL) {
        return 0; // Empty command
    }

    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }

    add_to_history(args[0]);

    if (is_builtin(args[0])) {
        run_builtin(args);
        return last_exit_status;
    } else {
        pid_t pid = fork();
        if (pid == 0) { // Child process
            execvp(args[0], args);
            perror("shush");
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            perror("shush: fork failed");
            return -1;
        } else {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                last_exit_status = WEXITSTATUS(status);
            } else {
                last_exit_status = 1; // Non-zero exit status if not exited normally
            }
            return last_exit_status;
        }
    }
}

// Utility function to trim leading and trailing whitespace
static char *trim_whitespace(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Null terminate
    end[1] = '\0';

    return str;
}

// Parse and execute commands with logical operators
void parse_and_execute(char *line) {
    char *cmd_start = line;
    char *cmd_end;
    int status = 0;
    bool execute_next = true;

    while (*cmd_start) {
        // Skip leading spaces
        cmd_start = trim_whitespace(cmd_start);
        if (*cmd_start == '\0') break;

        // Find the end of the current command
        cmd_end = cmd_start;
        while (*cmd_end && *cmd_end != ';' && *cmd_end != '&' && *cmd_end != '|') {
            if (*cmd_end == '"') {
                cmd_end++; // Skip the opening quote
                while (*cmd_end && *cmd_end != '"') cmd_end++;
                if (*cmd_end) cmd_end++; // Skip the closing quote
            } else {
                cmd_end++;
            }
        }

        // Save operator for next command
        char operator = *cmd_end;
        if (*cmd_end) {
            *cmd_end = '\0'; // Null-terminate the current command
            cmd_end++;
        }

        // Trim trailing spaces
        char *cmd = trim_whitespace(cmd_start);

        // Execute the command
        if (execute_next && *cmd) {
            char *expanded_command = expand_variables(cmd);
            if (is_directory(expanded_command)) {
                printf("shush: %s: Is a directory\n", expanded_command);
            } else {
                status = execute_command(expanded_command);
            }
            free(expanded_command);
        }

        // Determine if we need to execute the next command based on the operator
        if (operator == '&') {
            if (*cmd_end == '&') {
                // Logical AND (&&)
                execute_next = (status == 0);
                cmd_end++; // Skip over "&&"
            } else {
                execute_next = true; // Always execute the next command
            }
        } else if (operator == '|') {
            if (*cmd_end == '|') {
                // Logical OR (||)
                execute_next = (status != 0);
                cmd_end++; // Skip over "||"
            } else {
                execute_next = true; // Always execute the next command
            }
        } else if (operator == ';') {
            // Semicolon - always execute the next command
            execute_next = true;
        } else {
            execute_next = true; // Default to true if no operator
        }

        // Update command start for the next iteration
        cmd_start = cmd_end;
    }
}

// Expand variables (e.g., $HOME) in the command string
char *expand_variables(const char *input) {
    size_t len = strlen(input);
    char *expanded = malloc(len + 1);
    if (expanded == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    strcpy(expanded, input);

    char *result = malloc(len + 1);  // Allocate initially the same size as input
    if (result == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    size_t result_len = 0;

    for (size_t i = 0; i < len; i++) {
        if (expanded[i] == '~') {
            // Handle home directory
            if (home_directory != NULL) {
                size_t home_len = strlen(home_directory);
                result = realloc(result, result_len + home_len + 1);
                if (result == NULL) {
                    perror("realloc");
                    exit(EXIT_FAILURE);
                }
                strcpy(result + result_len, home_directory);
                result_len += home_len;
            } else {
                fprintf(stderr, "shush: HOME not set\n");
            }
        } else if (expanded[i] == '$' && i + 1 < len) {
            char *env_var_name = expanded + i + 1;
            char *end = env_var_name;
            
            // Extract variable name
            while (*end && (isalnum(*end) || *end == '_')) end++;
            
            size_t var_name_len = end - env_var_name;
            if (var_name_len > 0) {
                char var_name[var_name_len + 1];
                strncpy(var_name, env_var_name, var_name_len);
                var_name[var_name_len] = '\0';

                // Get variable value
                char *var_value = getenv(var_name);
                if (var_value != NULL) {
                    size_t value_len = strlen(var_value);
                    result = realloc(result, result_len + value_len + 1);
                    if (result == NULL) {
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }
                    strcpy(result + result_len, var_value);
                    result_len += value_len;
                }
                i += var_name_len; // Skip past the variable name
            }
        } else {
            // Copy character as is
            result = realloc(result, result_len + 2);
            if (result == NULL) {
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
