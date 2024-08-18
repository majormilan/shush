#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "builtins.h"
#include "shush.h"
#include "parse.h"

#define MAX_LINE 80

// Shell variables
int last_exit_status = 0;
char *home_directory;
int verbose = 1; // Could be set via command-line argument or environment variable

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
    //print_environment_variables();
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
