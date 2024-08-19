#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <linenoise.h>
#include "builtins.h"
#include "shush.h"
#include "parse.h"

#define MAX_LINE 80
#define MAX_PROMPT_LENGTH 1024

// Shell variables
int last_exit_status = 0;
char *home_directory;
int verbose = 1; // Could be set via command-line argument or environment variable

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
}

void update_prompt(char *prompt, size_t size) {
    char cwd[1024];
    const char *user = getenv("USER");
    const char *hostname = getenv("HOSTNAME");

    if (user == NULL) user = "user";
    if (hostname == NULL) hostname = "hostname";

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        strcpy(cwd, "[unknown]");
    }

    snprintf(prompt, size, "[%s@%s %s]$ ", user, hostname, cwd);
}

int main() {
    char *line;
    char prompt[MAX_PROMPT_LENGTH];

    initialize_shell();

    while (1) {
        // Update the prompt with the current working directory
        update_prompt(prompt, sizeof(prompt));

        // Use linenoise to read input with the updated prompt
        line = linenoise(prompt);
        if (line == NULL) break; // EOF or error

        // Expand variables in the line
        char *expanded_line = expand_variables(line);
        parse_and_execute(expanded_line);
        free(expanded_line);

        linenoiseHistoryAdd(line); // Add to history
        linenoiseHistorySave("history.txt"); // Save history

        free(line); // Free the line buffer
    }

    return 0;
}
