#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "builtins.h"
#include <ctype.h>

// Shell variables
#define MAX_HISTORY 100

// History array and count
char *history[MAX_HISTORY];
int history_count = 0;

// Add a command to history
void add_to_history(const char *command) {
    if (history_count < MAX_HISTORY) {
        history[history_count] = strdup(command);
        history_count++;
    } else {
        // Shift history and add new command
        free(history[0]);
        memmove(history, history + 1, (MAX_HISTORY - 1) * sizeof(char *));
        history[MAX_HISTORY - 1] = strdup(command);
    }
}

// Built-in echo command with -n option
void builtin_echo(char *args[]) {
    int newline = 1; // By default, add a newline
    int i = 1;

    // Check if the first argument is -n
    if (args[1] != NULL && strcmp(args[1], "-n") == 0) {
        newline = 0;
        i = 2; // Start printing from the second argument
    }

    for (; args[i] != NULL; i++) {
        char *expanded_arg = expand_variables(args[i]);
        printf("%s", expanded_arg);
        free(expanded_arg);
        if (args[i + 1] != NULL) {
            printf(" ");
        }
    }
    
    if (newline) {
        printf("\n");
    }
}

void builtin_history(char *args[]) {
    if (args[1] != NULL && strcmp(args[1], "-c") == 0) {
        // Clear history
        for (int i = 0; i < history_count; i++) {
            free(history[i]);
        }
        history_count = 0;
    } else {
        for (int i = 0; i < history_count; i++) {
            printf("%d %s\n", i + 1, history[i]);
        }
    }
}

// Built-in cd command
void builtin_cd(char *args[]) {
    if (args[1] == NULL) {
        // No argument, change to the home directory
        if (chdir(home_directory) != 0) {
            perror("shush");
            last_exit_status = 1;
        } else {
            last_exit_status = 0;
        }
    } else {
        // Change to the directory specified in args[1]
        if (chdir(args[1]) != 0) {
            perror("shush");
            last_exit_status = 1;
        } else {
            last_exit_status = 0;
        }
    }
}

// Built-in ver command
void builtin_ver(char *args[]) {
    printf("shush version 1.0\n");
    last_exit_status = 0;
}

// Function to expand shell variables
char *expand_variables(const char *input) {
    size_t len = strlen(input) + 1;
    char *expanded = malloc(len);
    if (expanded == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    strcpy(expanded, input);

    char *new_str = malloc(len);  // Initially allocate the same size as input
    if (new_str == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    size_t new_str_len = 0;

    for (size_t i = 0; i < strlen(expanded); i++) {
        if (expanded[i] == '~') {
            // Replace ~ with home directory
            if (home_directory != NULL) {
                size_t home_len = strlen(home_directory);
                new_str = realloc(new_str, new_str_len + home_len + 1);
                if (new_str == NULL) {
                    perror("realloc");
                    exit(EXIT_FAILURE);
                }
                strcpy(new_str + new_str_len, home_directory);
                new_str_len += home_len;
            } else {
                fprintf(stderr, "shush: HOME not set\n");
            }
        } else if (expanded[i] == '$' && i + 1 < strlen(expanded)) {
            char *env_var_name = expanded + i + 1;
            char *end = env_var_name;
            
            // Extract environment variable name
            while (*end && (isalnum(*end) || *end == '_')) end++;
            
            size_t var_name_len = end - env_var_name;
            if (var_name_len > 0) {
                char var_name[var_name_len + 1];
                strncpy(var_name, env_var_name, var_name_len);
                var_name[var_name_len] = '\0';

                // Retrieve environment variable value
                char *var_value = getenv(var_name);
                if (var_value != NULL) {
                    size_t value_len = strlen(var_value);
                    new_str = realloc(new_str, new_str_len + value_len + 1);
                    if (new_str == NULL) {
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }
                    strcpy(new_str + new_str_len, var_value);
                    new_str_len += value_len;
                }
                i += var_name_len;
            }
        } else {
            // Copy the character as is
            new_str = realloc(new_str, new_str_len + 2);
            if (new_str == NULL) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            new_str[new_str_len++] = expanded[i];
        }
    }

    // Null-terminate the new string
    new_str[new_str_len] = '\0';

    free(expanded);
    return new_str;
}
