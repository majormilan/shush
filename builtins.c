#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "builtins.h"

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

// Built-in echo command
void builtin_echo(char *args[]) {
    for (int i = 1; args[i] != NULL; i++) {
        char *expanded_arg = expand_variables(args[i]);
        printf("%s", expanded_arg);
        free(expanded_arg);
        if (args[i + 1] != NULL) {
            printf(" ");
        }
    }
    printf("\n");
}

// Built-in history command
void builtin_history(char *args[]) {
    for (int i = 0; i < history_count; i++) {
        printf("%d %s\n", i + 1, history[i]);
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
            size_t home_len = strlen(home_directory);
            new_str = realloc(new_str, new_str_len + home_len + 1);
            if (new_str == NULL) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            strcpy(new_str + new_str_len, home_directory);
            new_str_len += home_len;
        } else if (expanded[i] == '$' && i + 1 < strlen(expanded) && expanded[i + 1] == '?') {
            // Replace $? with last_exit_status
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%d", last_exit_status);
            size_t buffer_len = strlen(buffer);
            new_str = realloc(new_str, new_str_len + buffer_len + 1);
            if (new_str == NULL) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            strcpy(new_str + new_str_len, buffer);
            new_str_len += buffer_len;
            i++; // Skip the next character
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

