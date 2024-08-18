#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <signal.h>
#include "builtins.h"
#include "parse.h"

// Shell variables
#define MAX_HISTORY 100
#define MAX_ALIASES 100

// History array and count
char *history[MAX_HISTORY];
int history_count = 0;

// Alias storage
typedef struct {
    char *name;
    char *value;
} alias_t;

alias_t aliases[MAX_ALIASES];
int alias_count = 0;

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


// Function to check if a command is a built-in
bool is_builtin(const char *command) {
    for (int i = 0; command_table[i].name != NULL; i++) {
        if (strcmp(command, command_table[i].name) == 0) {
            return true;
        }
    }
    return false;
}

// Function to run a built-in command
void run_builtin(char *args[]) {
    for (int i = 0; command_table[i].name != NULL; i++) {
        if (strcmp(args[0], command_table[i].name) == 0) {
            command_table[i].func(args);
            return;
        }
    }
    fprintf(stderr, "Unknown built-in command: %s\n", args[0]);
    last_exit_status = 1;
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

// Built-in history command
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

// Built-in exit command
void builtin_exit(char *args[]) {
    int status = 0;
    if (args[1] != NULL) {
        status = atoi(args[1]);
    }
    exit(status);
}

// Built-in pwd command
void builtin_pwd(char *args[]) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
        last_exit_status = 1;
    }
}

// Built-in set command
void builtin_set(char *args[]) {
    for (int i = 1; args[i] != NULL; i++) {
        char *equal_sign = strchr(args[i], '=');
        if (equal_sign != NULL) {
            *equal_sign = '\0';
            setenv(args[i], equal_sign + 1, 1);
        } else {
            printf("Invalid format: %s\n", args[i]);
        }
    }
    last_exit_status = 0;
}

// Built-in unset command
void builtin_unset(char *args[]) {
    for (int i = 1; args[i] != NULL; i++) {
        unsetenv(args[i]);
    }
    last_exit_status = 0;
}

// Built-in export command
void builtin_export(char *args[]) {
    for (int i = 1; args[i] != NULL; i++) {
        char *equal_sign = strchr(args[i], '=');
        if (equal_sign != NULL) {
            *equal_sign = '\0';
            setenv(args[i], equal_sign + 1, 1);
        } else {
            printf("Invalid format: %s\n", args[i]);
        }
    }
    last_exit_status = 0;
}

// Built-in kill command
void builtin_kill(char *args[]) {
    if (args[1] != NULL && args[2] != NULL) {
        int pid = atoi(args[1]);
        int signal = atoi(args[2]);
        if (kill(pid, signal) == -1) {
            perror("kill");
            last_exit_status = 1;
        } else {
            last_exit_status = 0;
        }
    } else {
        fprintf(stderr, "Usage: kill <pid> <signal>\n");
        last_exit_status = 1;
    }
}

// Built-in alias command
void builtin_alias(char *args[]) {
    if (args[1] == NULL) {
        for (int i = 0; i < alias_count; i++) {
            printf("alias %s='%s'\n", aliases[i].name, aliases[i].value);
        }
    } else {
        for (int i = 1; args[i] != NULL; i++) {
            char *equal_sign = strchr(args[i], '=');
            if (equal_sign != NULL) {
                *equal_sign = '\0';
                // Store alias
                if (alias_count < MAX_ALIASES) {
                    aliases[alias_count].name = strdup(args[i]);
                    aliases[alias_count].value = strdup(equal_sign + 1);
                    alias_count++;
                } else {
                    printf("Alias limit reached.\n");
                }
            } else {
                printf("Invalid alias format: %s\n", args[i]);
            }
        }
    }
    last_exit_status = 0;
}

// Built-in unalias command
void builtin_unalias(char *args[]) {
    for (int i = 1; args[i] != NULL; i++) {
        for (int j = 0; j < alias_count; j++) {
            if (strcmp(args[i], aliases[j].name) == 0) {
                free(aliases[j].name);
                free(aliases[j].value);
                memmove(&aliases[j], &aliases[j + 1], (alias_count - j - 1) * sizeof(alias_t));
                alias_count--;
                break;
            }
        }
    }
    last_exit_status = 0;
}

// Built-in source command
void builtin_source(char *args[]) {
    if (args[1] != NULL) {
        FILE *file = fopen(args[1], "r");
        if (file == NULL) {
            perror("source");
            last_exit_status = 1;
            return;
        }

        char line[1024];
        while (fgets(line, sizeof(line), file)) {
            // Remove newline characters
            line[strcspn(line, "\n")] = '\0';
            // Execute the command
            parse_and_execute(line);
        }

        fclose(file);
        last_exit_status = 0;
    } else {
        fprintf(stderr, "Usage: source <file>\n");
        last_exit_status = 1;
    }
}

// Command table
const builtin_command command_table[] = {
    {"echo", builtin_echo},
    {"history", builtin_history},
    {"cd", builtin_cd},
    {"ver", builtin_ver},
    {"exit", builtin_exit},
    {"pwd", builtin_pwd},
    {"set", builtin_set},
    {"unset", builtin_unset},
    {"export", builtin_export},
    {"kill", builtin_kill},
    {"alias", builtin_alias},
    {"unalias", builtin_unalias},
    {"source", builtin_source},
    {NULL, NULL} // Sentinel value to mark the end of the table
};
