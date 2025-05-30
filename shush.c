/*
 * MIT/X Consortium License
 * Simple Humane Shell (shush) main file.
 */
#include "builtins.h"
#include "init.h"
#include "libtinyio/signal.h"
#include "libtinyio/stdio.h"
#include "libtinyio/string.h"
#include "parse.h"
#include "session.h"
#include "terminal.h"
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAX_PROMPT_LENGTH 1024
#define MAX_INPUT_LENGTH 8192

static pid_t child_pid = -1;
int last_exit_status;
Session session;

/* Store script parameters */
char *script_name = NULL; /* $0 */
char **script_args = NULL; /* $1, $2, ... */
int script_argc = 0;


/* Signal handler for SIGINT */
static void handle_sigint(int sig)
{
    if (child_pid > 0)
    {
        /* Terminate the child process if it exists */
        kill(child_pid, SIGTERM);
        waitpid(child_pid, NULL, 0);
        child_pid = -1;
    }
    else
    {
        /* Otherwise, reset the prompt */
        fflush(stdout);
        char prompt[MAX_PROMPT_LENGTH];
        update_prompt(prompt, sizeof(prompt));
        printf("\n%s", prompt); /*  Print the new prompt */
        fflush(stdout);
    }
}

/* Read multiline input from the terminal */
static char *read_multiline_input(void)
{
    char buffer[MAX_INPUT_LENGTH] = {0};
    size_t buffer_size = 0;
    char *line = NULL;

    while (1)
    {
        char prompt[MAX_PROMPT_LENGTH] = {0};
        if (isatty(fileno(stdin)))
        {
            update_prompt(prompt, sizeof(prompt));
            line = terminal_readline(prompt);
        }
        else
        {
            if (fgets(buffer + buffer_size, sizeof(buffer) - buffer_size,
                      stdin) == NULL)
            {
                if (buffer_size == 0)
                    return NULL;
                break;
            }
            line = strdup(buffer + buffer_size);
        }

        if (!line)
        {
            if (buffer_size == 0)
                return NULL;
            break;
        }

        size_t line_length = strlen(line);
        if (buffer_size + line_length >= MAX_INPUT_LENGTH)
        {
            fputs("Input exceeds maximum length.\n", stderr);
            free(line);
            return NULL;
        }

        memcpy(buffer + buffer_size, line, line_length);
        buffer_size += line_length;
        free(line);

        if (buffer_size > 0 && buffer[buffer_size - 1] == '\\')
        {
            buffer_size--;
            if (isatty(fileno(stdin)))
            {
                prompt[0] = '\0'; /* Continuation prompt */
            }
        }
        else
        {
            break;
        }
    }

    buffer[buffer_size] = '\0';
    if (buffer_size > 0)
    {
        fflush(stdout);
    }
    return strdup(buffer);
}

int main(int argc, char *argv[])
{
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    initialize_shell();
    initialize_session(&session);

    /* Handle script execution */
    if (argc > 1) {
        /* Store script name and arguments */
        script_name = strdup(argv[1]); /* $0 */
        script_argc = argc - 2; /* Number of positional arguments */
        script_args = malloc((script_argc + 1) * sizeof(char *));
        if (!script_name || !script_args) {
            perror("malloc");
            return 1;
        }
        for (int i = 0; i < script_argc; i++) {
            script_args[i] = strdup(argv[i + 2]);
            if (!script_args[i]) {
                perror("strdup");
                return 1;
            }
        }
        script_args[script_argc] = NULL;

        FILE *file = fopen(argv[1], "r");
        if (!file) {
            perror(argv[1]);
            return 1;
        }
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        last_exit_status = 0;
        while ((read = getline(&line, &len, file)) != -1) {
            line[strcspn(line, "\n")] = '\0';
            char *trimmed = line;
            while (isspace((unsigned char)*trimmed)) trimmed++;
            if (!*trimmed || *trimmed == '#') {
                continue;
            }
            parse_and_execute(trimmed);
        }
        free(line);
        fclose(file);

        /* Clean up script parameters */
        free(script_name);
        for (int i = 0; i < script_argc; i++) {
            free(script_args[i]);
        }
        free(script_args);
        return last_exit_status;
    }

    /* Interactive mode */
    while (1) {
        char *line = read_multiline_input();
        if (!line) {
            if (feof(stdin))
                break;
            continue;
        }
        update_session(&session);
        parse_and_execute(line);
        free(line);
        update_session(&session);
    }

    return last_exit_status;
}
