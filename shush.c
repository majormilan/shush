/*
 * MIT/X Consortium License
 * Simple Humane Shell (shush) main file.
 */
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "builtins.h"
#include "init.h"
#include "libtinyio/stdio.h"
#include "parse.h"
#include "session.h"
#include "terminal.h"
#include "libtinyio/signal.h"
#include "libtinyio/string.h"
#define MAX_PROMPT_LENGTH 1024
#define MAX_INPUT_LENGTH 8192

static pid_t child_pid = -1;
int last_exit_status;
Session session;

/* Signal handler for SIGINT */
static void handle_sigint(int sig) {
    if (child_pid > 0) {
        /* Terminate the child process if it exists */
        kill(child_pid, SIGTERM);
        waitpid(child_pid, NULL, 0);
        child_pid = -1;
    } else {
        /* Otherwise, reset the prompt */
        fflush(stdout);
        char prompt[MAX_PROMPT_LENGTH];
        update_prompt(prompt, sizeof(prompt));
        printf("\n%s", prompt);  // Print the new prompt
        fflush(stdout);
    }
}

/* Read multiline input from the terminal */
static char *read_multiline_input(void) {
    char buffer[MAX_INPUT_LENGTH] = {0};
    size_t buffer_size = 0;
    char *line = NULL;

    while (1) {
        char prompt[MAX_PROMPT_LENGTH] = {0};
        if (isatty(fileno(stdin))) {
            update_prompt(prompt, sizeof(prompt));
            line = terminal_readline(prompt);
        } else {
            if (fgets(buffer + buffer_size, sizeof(buffer) - buffer_size, stdin) == NULL) {
                if (buffer_size == 0) return NULL;
                break;
            }
            line = strdup(buffer + buffer_size);
        }

        if (!line) {
            if (buffer_size == 0) return NULL;
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
            if (isatty(fileno(stdin))) {
                prompt[0] = '\0'; /* Continuation prompt */
            }
        } else {
            break;
        }
    }

    buffer[buffer_size] = '\0';
    if (buffer_size > 0) {
        fflush(stdout);
    }
    return strdup(buffer);
}

int main(int argc, char *argv[]) {
    /* Set up the SIGINT signal handler */
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // Ensure interrupted system calls are restarted
    sigaction(SIGINT, &sa, NULL);

    initialize_shell();
    initialize_session(&session);

    while (1) {
        char *line = read_multiline_input();
        if (!line) {
            if (feof(stdin)) break;
            continue;
        }

        update_session(&session);
        parse_and_execute(line);
        free(line);
        update_session(&session);
    }

    return 0;
}
