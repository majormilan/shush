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
#define MAX_PATH_LEN 4096

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
    (void)sig; /* required by the sigaction handler signature, unused */
    if (child_pid > 0)
    {
        kill(child_pid, SIGTERM);
        waitpid(child_pid, NULL, 0);
        child_pid = -1;
    }
    else
    {
        fflush(stdout);
        char prompt[MAX_PROMPT_LENGTH];
        update_prompt(prompt, sizeof(prompt));
        printf("\n%s", prompt);
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

/* $0 and the positional parameters $1, $2, ... */
static int set_params(const char *name, int argc, char **argv)
{
    script_name = strdup(name);
    script_argc = argc;
    script_args = malloc((argc + 1) * sizeof(char *));
    if (!script_name || !script_args) {
        perror("malloc");
        return -1;
    }
    for (int i = 0; i < argc; i++) {
        if (!(script_args[i] = strdup(argv[i]))) {
            perror("strdup");
            return -1;
        }
    }
    script_args[argc] = NULL;
    return 0;
}

static void free_params(void)
{
    free(script_name);
    for (int i = 0; i < script_argc; i++) {
        free(script_args[i]);
    }
    free(script_args);
}

/* One line of a script, a -c string or ~/.shushrc */
static void run_line(char *line)
{
    while (isspace((unsigned char)*line)) line++;
    if (*line && *line != '#') {
        parse_and_execute(line);
    }
}

static void run_file(FILE *file)
{
    char *line = NULL;
    size_t len = 0;

    while (getline(&line, &len, file) != -1) {
        line[strcspn(line, "\n")] = '\0';
        run_line(line);
    }
    free(line);
}

static void run_string(const char *s)
{
    char *copy = strdup(s), *line, *next;

    if (!copy) {
        perror("strdup");
        return;
    }
    for (line = copy; line; line = next) {
        if ((next = strchr(line, '\n'))) {
            *next++ = '\0';
        }
        run_line(line);
    }
    free(copy);
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

    /* Handle --version flag */
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        char *ver_args[] = {"ver", NULL};
        builtin_ver(ver_args);
        return last_exit_status;
    }

    /*
     * shush [-l] [script [args...]]
     * shush [-l] -c string [name [args...]]
     * shush [-l] -s [args...]
     * -l (--login): a login shell, as login(1) starts one.
     */
    const char *command = NULL;
    int from_stdin = 0, i = 1;
    while (i < argc && argv[i][0] == '-' && argv[i][1]) {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--login") == 0) {
            i++;
        } else if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc) {
                fputs("shush: -c: option requires an argument\n", stderr);
                return 2;
            }
            command = argv[i + 1];
            i += 2;
            break;
        } else if (strcmp(argv[i], "-s") == 0) {
            from_stdin = 1;
            i++;
            break;
        } else {
            fprintf(stderr, "shush: %s: invalid option\n", argv[i]);
            return 2;
        }
    }

    if (command) {
        if (set_params(i < argc ? argv[i] : argv[0], i < argc ? argc - i - 1 : 0,
                       argv + i + 1) < 0) {
            return 1;
        }
        last_exit_status = 0;
        run_string(command);
        free_params();
        return last_exit_status;
    }

    /* Handle script execution */
    if (!from_stdin && i < argc) {
        if (set_params(argv[i], argc - i - 1, argv + i + 1) < 0) {
            return 1;
        }
        FILE *file = fopen(argv[i], "r");
        if (!file) {
            perror(argv[i]);
            return 127;
        }
        last_exit_status = 0;
        run_file(file);
        fclose(file);
        free_params();
        return last_exit_status;
    }
    if (from_stdin && set_params(argv[0], argc - i, argv + i) < 0) {
        return 1;
    }

    /* Source startup file in interactive mode */
    if (isatty(STDIN_FILENO)) {
        char shushrc_path[MAX_PATH_LEN];
        const char *home = getenv("HOME");
        if (home) {
            snprintf(shushrc_path, sizeof(shushrc_path), "%s/.shushrc", home);
            FILE *file = fopen(shushrc_path, "r");
            if (file) {
                run_file(file);
                fclose(file);
            }
        }
    }

    /* Interactive mode */
    while (1) {
        char *line = read_multiline_input();
        if (!line) {
            if (feof(stdin)) {
                extern pid_t bg_procs[];
                extern int bg_proc_count;
                for (int i = 0; i < bg_proc_count; i++) {
                    kill(bg_procs[i], SIGTERM);
                    waitpid(bg_procs[i], NULL, 0);
                }
                break;
            }
            continue;
        }
        update_session(&session);
        parse_and_execute(line);
        free(line);
        update_session(&session);
    }

    return last_exit_status;
}
