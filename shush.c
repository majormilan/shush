#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <linenoise.h>
#include "builtins.h"
#include "shush.h"
#include "parse.h"

#define MAX_PROMPT_LENGTH 1024

// Shell variables
int last_exit_status = 0;
char *home_directory;
pid_t child_pid = -1; // Track child process ID

// Signal handler for SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    if (child_pid > 0) {
        // Terminate the running child process
        kill(child_pid, SIGTERM);
        waitpid(child_pid, NULL, 0);
        child_pid = -1; // Reset child process ID
    } else {
        // Print a new prompt
        // This will handle the case when SIGINT is received while waiting for input
        // linenoise should handle this scenario and return NULL on error
        printf("\n");
        fflush(stdout);
    }
}

// Initialize shell environment
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

// Update prompt with current working directory
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

    // Set up the signal handler for SIGINT (Ctrl+C)
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; // Handle signal interrupts
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    initialize_shell();

    while (1) {
        // Update the prompt with the current working directory
        update_prompt(prompt, sizeof(prompt));

        // Use linenoise to read input with the updated prompt
        line = linenoise(prompt);
        if (line == NULL) {
            // linenoise returns NULL on EOF or error
            continue; // Loop back to display the prompt again
        }

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
