/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Simple Humane Shell (shush) main file.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "init.h"
#include "parse.h"
#include "terminal.h"

#define MAX_PROMPT_LENGTH  1024
#define MAX_INPUT_LENGTH   8192

static pid_t child_pid = -1;
int last_exit_status;

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

        char prompt[MAX_PROMPT_LENGTH];
        update_prompt(prompt, sizeof(prompt));
        printf("%s", prompt);
        fflush(stdout);
    }
}

static char *
read_multiline_input(void)
{
    char buffer[MAX_INPUT_LENGTH];
    size_t buffer_size = 0;
    char *line = NULL;

    while (1) {
        char prompt[MAX_PROMPT_LENGTH];
        update_prompt(prompt, sizeof(prompt));
        line = terminal_readline(prompt);

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
            prompt[0] = '\0'; // Continuation prompt
        } else {
            break;
        }
    }

    buffer[buffer_size] = '\0';

    if (buffer_size > 0) {
        putchar('\n'); // Print a newline after processing input
        fflush(stdout);
    }

    return strdup(buffer);
}

int
main(int argc, char *argv[])
{
    signal(SIGINT, handle_sigint);
    initialize_shell();

    while (1) {
        char *line = read_multiline_input();
        if (!line) {
            if (feof(stdin))
                break;
            continue;
        }

        parse_and_execute(line);
        free(line);
    }

    return 0;
}
