/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Built-in commands for Simple Humane Shell (shush).
 */

#include "builtins.h"
#include "init.h"
#include "libtinyio/stdio.h"
#include "parse.h"
#include <ctype.h>
#include <signal.h>
/* #include <stdio.h> */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* Shell variables */
#define MAX_HISTORY 100
#define MAX_ALIASES 100
#define MAX_ARGS 128

/* History array and count */
char *history[MAX_HISTORY];
int history_count = 0;

/* Alias storage */
typedef struct
{
    char *name;
    char *value;
} alias_t;

static alias_t aliases[MAX_ALIASES];
static int alias_count = 0;

/* Function declarations */
void add_to_history(const char *command);
bool is_builtin(const char *command);
void run_builtin(char *args[]);
void builtin_echo(char *args[]);
void builtin_history(char *args[]);
void builtin_cd(char *args[]);
void builtin_ver(char *args[]);
void builtin_exit(char *args[]);
void builtin_pwd(char *args[]);
void builtin_set(char *args[]);
void builtin_unset(char *args[]);
void builtin_export(char *args[]);
void builtin_kill(char *args[]);
void builtin_alias(char *args[]);
void builtin_unalias(char *args[]);
void builtin_source(char *args[]);

const char *custom_strsignal(int sig)
{
    static const char *signals[] = {
        [SIGHUP] = "Hangup",
        [SIGINT] = "Interrupt",
        [SIGQUIT] = "Quit",
        [SIGILL] = "Illegal instruction",
        [SIGTRAP] = "Trace/breakpoint trap",
        [SIGABRT] = "Aborted",
        [SIGBUS] = "Bus error",
        [SIGFPE] = "Floating point exception",
        [SIGKILL] = "Killed",
        [SIGUSR1] = "User defined signal 1",
        [SIGSEGV] = "Segmentation fault",
        [SIGUSR2] = "User defined signal 2",
        [SIGPIPE] = "Broken pipe",
        [SIGALRM] = "Alarm clock",
        [SIGTERM] = "Terminated",
        [SIGSTKFLT] = "Stack fault",
        [SIGCHLD] = "Child exited",
        [SIGCONT] = "Continue",
        [SIGSTOP] = "Stop",
        [SIGTSTP] = "Terminal stop",
        [SIGTTIN] = "Background read from tty",
        [SIGTTOU] = "Background write to tty",
        [SIGURG] = "Urgent condition on socket",
        [SIGXCPU] = "CPU time limit exceeded",
        [SIGXFSZ] = "File size limit exceeded",
        [SIGVTALRM] = "Virtual alarm clock",
        [SIGPROF] = "Profiling timer expired",
        [SIGWINCH] = "Window size change",
        [SIGIO] = "I/O possible",
        [SIGPWR] = "Power failure",
        [SIGSYS] = "Bad system call",
    };

    if (sig >= 1 && sig < sizeof(signals) / sizeof(signals[0]) && signals[sig])
        return signals[sig];
    return "Unknown signal";
}

/* Add a command to history */
void add_to_history(const char *command)
{
    if (history_count < MAX_HISTORY)
    {
        history[history_count++] = strdup(command);
    }
    else
    {
        free(history[0]);
        memmove(history, history + 1, (MAX_HISTORY - 1) * sizeof(char *));
        history[MAX_HISTORY - 1] = strdup(command);
    }
}

/* Check if a command is a built-in */
bool is_builtin(const char *command)
{
    for (int i = 0; command_table[i].name; i++)
    {
        if (!strcmp(command, command_table[i].name))
            return true;
    }
    return false;
}

/* Run a built-in command */
void run_builtin(char *args[])
{
    for (int i = 0; command_table[i].name; i++)
    {
        if (!strcmp(args[0], command_table[i].name))
        {
            command_table[i].func(args);
            return;
        }
    }
    fprintf(stderr, "Unknown built-in command: %s\n", args[0]);
    last_exit_status = 1;
}

/* Built-in echo command */
void builtin_echo(char *args[])
{
    int newline = 1;
    int interpret_escapes = 0;
    int i = 1;

    while (args[i] && args[i][0] == '-')
    {
        if (!strcmp(args[i], "-n"))
        {
            newline = 0;
        }
        else if (!strcmp(args[i], "-e"))
        {
            interpret_escapes = 1;
        }
        else if (!strcmp(args[i], "-E"))
        {
            interpret_escapes = 0;
        }
        else if (!strcmp(args[i], "--help"))
        {
            printf("echo: echo [-neE] [string ...]\n");
            printf("    Write arguments to the standard output.\n\n");
            printf("    Options:\n");
            printf("      -n    do not output the trailing newline\n");
            printf("      -e    enable interpretation of backslash escapes\n");
            printf("      -E    disable interpretation of backslash escapes "
                   "(default)\n");
            last_exit_status = 0;
            return;
        }
        else
        {
            fprintf(stderr, "echo: invalid option -- '%s'\n", args[i]);
            last_exit_status = 1;
            return;
        }
        i++;
    }

    for (; args[i]; i++)
    {
        if (interpret_escapes)
        {
            for (char *p = args[i]; *p; p++)
            {
                if (*p == '\\')
                {
                    switch (*(++p))
                    {
                        case 'n':
                            putchar('\n');
                            break;
                        case 't':
                            putchar('\t');
                            break;
                        case 'r':
                            putchar('\r');
                            break;
                        case 'b':
                            putchar('\b');
                            break;
                        case '\\':
                            putchar('\\');
                            break;
                        case '\"':
                            putchar('\"');
                            break;
                        case '\'':
                            putchar('\'');
                            break;
                        default:
                            putchar('\\');
                            putchar(*p);
                            break;
                    }
                }
                else
                {
                    putchar(*p);
                }
            }
        }
        else
        {
            fputs(args[i], stdout);
        }

        if (args[i + 1])
            putchar(' ');
    }

    if (newline)
        putchar('\n');

    last_exit_status = 0;
}

/* Built-in history command */
void builtin_history(char *args[])
{
    if (args[1])
    {
        if (!strcmp(args[1], "-c"))
        {
            for (int i = 0; i < history_count; i++)
                free(history[i]);
            history_count = 0;
            last_exit_status = 0;
        }
        else if (!strcmp(args[1], "-d") && args[2])
        {
            int index = atoi(args[2]) - 1;
            if (index >= 0 && index < history_count)
            {
                free(history[index]);
                memmove(&history[index], &history[index + 1],
                        (history_count - index - 1) * sizeof(char *));
                history_count--;
                last_exit_status = 0;
            }
            else
            {
                fprintf(stderr, "history: %s: history position out of range\n",
                        args[2]);
                last_exit_status = 1;
            }
        }
        else
        {
            fprintf(stderr, "history: invalid option -- '%s'\n", args[1]);
            last_exit_status = 1;
        }
    }
    else
    {
        for (int i = 0; i < history_count; i++)
            printf("%d %s\n", i + 1, history[i]);
    }
}

/* Built-in cd command */
void builtin_cd(char *args[])
{
    char cwd[1024];
    char *target_dir =
        args[1] ? (strcmp(args[1], "-") == 0 ? getenv("OLDPWD") : args[1])
                : home_directory;

    if (!target_dir)
    {
        fprintf(stderr, "shush: cd: OLDPWD not set\n");
        last_exit_status = 1;
        return;
    }

    if (chdir(target_dir) != 0)
    {
        perror("shush");
        last_exit_status = 1;
        return;
    }

    char *pwd = getcwd(cwd, sizeof(cwd));
    if (!pwd)
    {
        perror("getcwd");
        last_exit_status = 1;
        return;
    }

    setenv("OLDPWD", getenv("PWD"), 1);
    setenv("PWD", pwd, 1);
    last_exit_status = 0;
}

/* Built-in ver command */
void builtin_ver(char *args[])
{
    printf("shush version 1.0\n");
    last_exit_status = 0;
}

/* Built-in exit command */
void builtin_exit(char *args[])
{
    int status = last_exit_status;
    if (args[1])
    {
        char *endptr;
        status = strtol(args[1], &endptr, 10);
        if (*endptr)
        {
            fprintf(stderr, "exit: %s: numeric argument required\n", args[1]);
            status = 1;
        }
    }
    exit(status);
}

/* Built-in pwd command */
void builtin_pwd(char *args[])
{
    int logical = args[1] && !strcmp(args[1], "-P");
    if (logical)
    {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)))
            printf("%s\n", cwd);
        else
        {
            perror("pwd");
            last_exit_status = 1;
        }
    }
    else
    {
        char *pwd = getenv("PWD");
        if (pwd)
            printf("%s\n", pwd);
        else
        {
            perror("pwd");
            last_exit_status = 1;
        }
    }
}

/* Built-in set command */
void builtin_set(char *args[])
{
    if (args[1])
    {
        fprintf(stderr, "set: Invalid usage\n");
        last_exit_status = 1;
    }
    else
    {
        extern char **environ;
        for (char **env = environ; *env; ++env)
            printf("%s\n", *env);
        last_exit_status = 0;
    }
}

/* Built-in unset command */
void builtin_unset(char *args[])
{
    for (int i = 1; args[i]; i++)
    {
        if (unsetenv(args[i]))
        {
            fprintf(stderr, "unset: %s: cannot unset\n", args[i]);
            last_exit_status = 1;
        }
    }
}

/* Built-in export command */
void builtin_export(char *args[])
{
    for (int i = 1; args[i]; i++)
    {
        if (putenv(args[i]))
        {
            fprintf(stderr, "export: %s: export failed\n", args[i]);
            last_exit_status = 1;
        }
    }
}

/* Built-in kill command */
void builtin_kill(char *args[])
{
    if (!args[1])
    {
        fprintf(stderr, "kill: process ID required\n");
        last_exit_status = 1;
        return;
    }

    pid_t pid = atoi(args[1]);
    int sig = SIGTERM;
    if (args[2])
    {
        int signum = custom_strsignal(sig);
        if (signum)
        {
            sig = signum;
        }
        else
        {
            fprintf(stderr, "kill: invalid signal -- '%s'\n", args[2]);
            last_exit_status = 1;
            return;
        }
    }

    if (kill(pid, sig) == -1)
    {
        perror("kill");
        last_exit_status = 1;
    }
    else
    {
        last_exit_status = 0;
    }
}

/* Built-in alias command */
void builtin_alias(char *args[])
{
    if (!args[1])
    {
        for (int i = 0; i < alias_count; i++)
        {
            printf("%s='%s'\n", aliases[i].name, aliases[i].value);
        }
        last_exit_status = 0;
    }
    else if (!strcmp(args[1], "-d") && args[2])
    {
        for (int i = 0; i < alias_count; i++)
        {
            if (!strcmp(aliases[i].name, args[2]))
            {
                free(aliases[i].name);
                free(aliases[i].value);
                memmove(&aliases[i], &aliases[i + 1],
                        (alias_count - i - 1) * sizeof(alias_t));
                alias_count--;
                last_exit_status = 0;
                return;
            }
        }
        fprintf(stderr, "alias: '%s' not found\n", args[2]);
        last_exit_status = 1;
    }
    else
    {
        char *name = strdup(args[1]);
        char *value = strdup(args[2]);
        if (name && value)
        {
            aliases[alias_count++] = (alias_t){name, value};
            last_exit_status = 0;
        }
        else
        {
            fprintf(stderr, "alias: could not create alias\n");
            last_exit_status = 1;
        }
    }
}

/* Built-in unalias command */
void builtin_unalias(char *args[])
{
    if (!args[1])
    {
        fprintf(stderr, "unalias: missing argument\n");
        last_exit_status = 1;
    }
    else
    {
        for (int i = 0; i < alias_count; i++)
        {
            if (!strcmp(aliases[i].name, args[1]))
            {
                free(aliases[i].name);
                free(aliases[i].value);
                memmove(&aliases[i], &aliases[i + 1],
                        (alias_count - i - 1) * sizeof(alias_t));
                alias_count--;
                last_exit_status = 0;
                return;
            }
        }
        fprintf(stderr, "unalias: '%s' not found\n", args[1]);
        last_exit_status = 1;
    }
}

/* Built-in source command */
void builtin_source(char *args[])
{
    if (!args[1])
    {
        fprintf(stderr, "source: file not specified\n");
        last_exit_status = 1;
    }
    else
    {
        FILE *file = fopen(args[1], "r");
        if (!file)
        {
            perror("source");
            last_exit_status = 1;
        }
        else
        {
            char line[1024];
            while (fgets(line, sizeof(line), file))
            {
                /*  Process the line */
                printf("Processing line: %s", line);
            }
            fclose(file);
            last_exit_status = 0;
        }
    }
}

/* Command table */
const builtin_command_t command_table[] = {
    {"echo", builtin_echo},
    {"history", builtin_history},
    {"cd", builtin_cd},
    {"ver", builtin_ver},
    {"exit", builtin_exit},
    {"pwd", builtin_pwd},
    {"set", builtin_set},
    {"unset", builtin_unset},
    {"export", builtin_export},
    {"kill", builtin_kill},
    {"alias", builtin_alias},
    {"unalias", builtin_unalias},
    {"source", builtin_source},
    {NULL, NULL} /* Sentinel value to mark the end of the table */
};
