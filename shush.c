#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/stat.h>
#include "builtins.h"



#define MAX_LINE 80
#define DEBUG 1


// Shell variables
int last_exit_status = 0;
char *home_directory;
int verbose = 1; // Could be set via command-line argument or environment variable



// Define the command table
const builtin_command command_table[] = {
    {"echo", builtin_echo},
    {"history", builtin_history},
    {"cd", builtin_cd},
    {"ver", builtin_ver},
    {NULL, NULL} // Sentinel value to mark the end of the table
};

// Check if the command is a built-in command
bool is_builtin(const char *command) {
    for (const builtin_command *cmd = command_table; cmd->name != NULL; cmd++) {
        if (strcmp(command, cmd->name) == 0) {
            return true;
        }
    }
    return false;
}

// Execute a built-in command
void run_builtin(char *args[]) {
    for (const builtin_command *cmd = command_table; cmd->name != NULL; cmd++) {
        if (strcmp(args[0], cmd->name) == 0) {
            cmd->func(args);
            return;
        }
    }
}


void print_environment_variables() {
    extern char **environ;
    char **env = environ;

    printf("Environment variables:\n");
    while (*env) {
        printf("%s\n", *env);
        env++;
    }
    printf("\n");
}

void initialize_shell() {
    home_directory = getenv("HOME");
    if (home_directory == NULL) {
        fprintf(stderr, "Error: HOME environment variable is not set.\n");
        exit(EXIT_FAILURE);
    }

    // Set default PATH
    const char *default_path = "/bin:/usr/bin";
    if (setenv("PATH", default_path, 1) != 0) {
        perror("setenv");
        exit(EXIT_FAILURE);
    }
    print_environment_variables();
}

// Utility function to check if a path is a directory
bool is_directory(const char *path) {
    struct stat path_stat;
    if (stat(path, &path_stat) == 0) {
        return S_ISDIR(path_stat.st_mode);
    }
    return false;
}


int execute_command(char *cmd) {
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

    if (DEBUG) {
        fprintf(stderr, "shush: PATH = %s\n", getenv("PATH"));
    }

    if (is_builtin(args[0])) {
        run_builtin(args);
        return last_exit_status;
    } else {
        pid_t pid = fork();
        if (pid == 0) { // Child process
            execvp(args[0], args);
            fprintf(stderr, "shush: failed to execute '%s': %s\n", args[0], strerror(errno));
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            fprintf(stderr, "shush: fork failed: %s\n", strerror(errno));
            return -1;
        } else {
            int status;
            waitpid(pid, &status, 0);
            last_exit_status = WEXITSTATUS(status);
            if (DEBUG) {
                fprintf(stderr, "shush: command '%s' exited with status %d\n", args[0], last_exit_status);
            }
            return last_exit_status;
        }
    }
}


// Print the shell prompt
void print_prompt() {
    char cwd[1024];
    const char *user = getenv("USER");
    const char *hostname = getenv("HOSTNAME");

    if (user == NULL) user = "user";
    if (hostname == NULL) hostname = "hostname";

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        strcpy(cwd, "[unknown]");
    }

    printf("[%s@%s %s]$ ", user, hostname, cwd);
    fflush(stdout); // Ensure the prompt is printed immediately
}

// Parse and execute commands with logical operators and pipes
void parse_and_execute(char *line) {
    char *command_list[MAX_LINE];
    char *cmd = strtok(line, "&|");
    int i = 0;

    // Tokenize the line by logical operators and pipes
    while (cmd != NULL) {
        command_list[i++] = cmd;
        cmd = strtok(NULL, "&|");
    }
    command_list[i] = NULL;

    // Execute commands considering logical operators
    int status = 0;
    for (int j = 0; command_list[j] != NULL; j++) {
        char *command = command_list[j];

        // Expand variables in the command
        char *expanded_command = expand_variables(command);

        // Print the expanded command
        printf("Command to execute: %s\n", expanded_command);

        // Check if the command is a directory
        if (is_directory(expanded_command)) {
            printf("shush: %s: Is a directory\n", expanded_command);
            free(expanded_command);
            continue;
        }

        // Execute the command
        int result = execute_command(expanded_command);

        // Free the expanded command
        free(expanded_command);

        // Handle logical operators
        if (strchr(line, '&') != NULL) {
            if (strchr(line, '&') != NULL && strchr(line, '&') == line + strlen(line) - 1) {
                // Execute next command only if the current command succeeded
                if (status != 0) {
                    break;
                }
            }
        } else if (strchr(line, '|') != NULL) {
            // Execute next command if the current command failed
            if (status == 0) {
                continue;
            }
        }

        status = result;
    }
}

int main() {
    char line[MAX_LINE];
    initialize_shell();
    while (1) {
        print_prompt(); // Print the enhanced prompt

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        // Remove newline character if present
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Expand variables in the line
        char *expanded_line = expand_variables(line);

        parse_and_execute(expanded_line);
        free(expanded_line);
    }

    // Free history memory
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
    }

    return 0;
}

