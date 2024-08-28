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
#define MAX_INPUT_LENGTH  8192

int last_exit_status;  // Correctly declared as external
static pid_t child_pid = -1;

static void handle_sigint(int sig);
static void update_prompt(char *prompt, size_t size);
static char *read_multiline_input(const char *prompt);

static void
handle_sigint(int sig)
{
	if (child_pid > 0) {
		kill(child_pid, SIGTERM);
		waitpid(child_pid, NULL, 0);
		child_pid = -1;
	} else {
		putchar('\n');
		fflush(stdout);
	}
}

static void
update_prompt(char *prompt, size_t size)
{
    char cwd[1024], temp[1024];
    const char *home = getenv("HOME");
    const char *user = getenv("USER") ? : "user";
    const char *hostname = getenv("HOSTNAME") ? : "localhost";

    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd");
        snprintf(cwd, sizeof(cwd), "[unknown]");
    } else if (home && strncmp(cwd, home, strlen(home)) == 0) {
        if (cwd[strlen(home)] == '\0')
            snprintf(temp, sizeof(temp), "~");
        else
            snprintf(temp, sizeof(temp), "~%s", cwd + strlen(home));
        strncpy(cwd, temp, sizeof(cwd) - 1);
        cwd[sizeof(cwd) - 1] = '\0';
    }

    int prompt_len;
    if (geteuid() == 0)
        prompt_len = snprintf(prompt, size, "[%s@%s %s]# ", user, hostname, cwd);
    else
        prompt_len = snprintf(prompt, size, "[%s@%s %s]$ ", user, hostname, cwd);

    if (prompt_len >= (int)size) {
        fprintf(stderr, "Warning: Prompt string truncated.\n");
        prompt[size - 1] = '\0';
    }
}

static char *
read_multiline_input(const char *prompt)
{
	char *line = NULL, buffer[MAX_INPUT_LENGTH];
	size_t buffer_size = 0;

	while (1) {
		line = linenoise(prompt);
		if (!line) {
			if (buffer_size == 0)
				return NULL;
			break;
		}

		size_t line_length = strlen(line);
		if (buffer_size + line_length >= MAX_INPUT_LENGTH) {
			fputs("Input exceeds maximum length.\n", stderr);
			free(line);
			return NULL;
		}
		memcpy(buffer + buffer_size, line, line_length);
		buffer_size += line_length;
		free(line);

		if (buffer_size > 0 && buffer[buffer_size - 1] == '\\') {
			buffer_size--;
			prompt = " > ";
		} else {
			break;
		}
	}

	buffer[buffer_size] = '\0';
	return strdup(buffer);
}

int
main(void)
{
	char *line, prompt[MAX_PROMPT_LENGTH];
	char history_file_path[1024];
	struct sigaction sa = {
		.sa_handler = handle_sigint,
		.sa_flags = SA_RESTART | SA_NOCLDSTOP
	};

	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

	initialize_shell();

	snprintf(history_file_path, sizeof(history_file_path), "%s/.shush_history", getenv("HOME"));
	linenoiseHistoryLoad(history_file_path);

	while (1) {
		update_prompt(prompt, sizeof(prompt));
		fflush(stdout);
		line = read_multiline_input(prompt);
		if (!line)
			continue;

		linenoiseHistoryAdd(line);
		linenoiseHistorySave(history_file_path);

		char *expanded_line = expand_variables(line);
		parse_and_execute(expanded_line);
		free(expanded_line);
		free(line);
	}

	return 0;
}

