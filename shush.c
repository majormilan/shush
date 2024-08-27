/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Main file for the Simple Humane Shell (shush).
 * Handles the main loop, signal handling, and prompt updates.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <linenoise.h>

#include "builtins.h"
#include "init.h"
#include "parse.h"
#include "shush.h"

#define MAX_PROMPT_LENGTH 1024

int last_exit_status = 0;
pid_t child_pid = -1;

static void handle_sigint(int sig);
static void update_prompt(char *prompt, size_t size);

static void
handle_sigint(int sig)
{
    if (child_pid > 0) {
        kill(child_pid, SIGTERM);
        waitpid(child_pid, NULL, 0);
        child_pid = -1;
    } else {
        printf("\n");
        fflush(stdout);
    }
}

static void
update_prompt(char *prompt, size_t size)
{
    char cwd[1024];
    const char *home = getenv("HOME");
    const char *user = getenv("USER");
    const char *hostname = getenv("HOSTNAME"); // Retrieve hostname from environment

    if (!user)
        user = "user";

    if (!hostname)
        hostname = "localhost"; // Fallback if HOSTNAME is not set

    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd");
        strcpy(cwd, "[unknown]");
    } else if (home && strncmp(cwd, home, strlen(home)) == 0) {
        // If the current directory is the home directory or a subdirectory of it
        if (cwd[strlen(home)] == '\0') {
            // If we are exactly in the home directory
            snprintf(cwd, sizeof(cwd), "~");
        } else {
            // If we are in a subdirectory of the home directory
            snprintf(cwd, sizeof(cwd), "~%s", cwd + strlen(home));
        }
    }

    // Check if the user is a superuser
    if (geteuid() == 0) {
        // Superuser: Use #
        snprintf(prompt, size, "[%s@%s %s]# ", user, hostname, cwd);
    } else {
        // Regular user: Use $
        snprintf(prompt, size, "[%s@%s %s]$ ", user, hostname, cwd);
    }
}

int
main(void)
{
    char *line;
    char prompt[MAX_PROMPT_LENGTH];
    char history_file_path[1024];

    struct sigaction sa = {
        .sa_handler = handle_sigint,
        .sa_flags = SA_RESTART | SA_NOCLDSTOP
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    initialize_shell();

    snprintf(history_file_path, sizeof(history_file_path), "%s/.shush_history", getenv("HOME"));

    while (1) {
        update_prompt(prompt, sizeof(prompt));
        line = linenoise(prompt);
        if (!line)
            continue;

        char *expanded_line = expand_variables(line);
        parse_and_execute(expanded_line);
        free(expanded_line);

        linenoiseHistoryAdd(line);
        linenoiseHistorySave(history_file_path);

        free(line);
    }

    return 0;
}
