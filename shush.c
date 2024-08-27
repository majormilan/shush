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
    char temp[1024];  
    const char *home = getenv("HOME");
    const char *user = getenv("USER");
    const char *hostname = getenv("HOSTNAME");

    if (!user)
        user = "user";

    if (!hostname)
        hostname = "localhost";

    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd");
        strcpy(cwd, "[unknown]");
    } else if (home && strncmp(cwd, home, strlen(home)) == 0) {
        if (cwd[strlen(home)] == '\0') {
            snprintf(temp, sizeof(temp), "~");
        } else {
            snprintf(temp, sizeof(temp), "~%s", cwd + strlen(home));
        }
        strncpy(cwd, temp, sizeof(cwd) - 1);
        cwd[sizeof(cwd) - 1] = '\0'; 
    }

    if (geteuid() == 0) {
        snprintf(prompt, size, "[%s@%s %s]# ", user, hostname, cwd);
    } else {
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
