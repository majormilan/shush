/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Built-in commands for Simple Humane Shell (shush).
 */

#include "builtins.h"
#include "init.h"
#include "libtinyio/signal.h"
#include "libtinyio/stdio.h"
#include "libtinyio/string.h"
#include "parse.h"
#include "session.h"
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

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

/* Add a command to history */
void add_to_history(const char *command)
{
    if (!command || !*command || isspace((unsigned char)*command))
    {
        return; /* Skip empty or whitespace-only commands */
    }
    char *cmd_copy = strdup(command);
    if (!cmd_copy)
    {
        fprintf(stderr, "shush: history: memory allocation failed\n");
        last_exit_status = 1;
        return;
    }
    if (history_count < MAX_HISTORY)
    {
        history[history_count++] = cmd_copy;
    }
    else
    {
        free(history[0]);
        memmove(history, history + 1, (MAX_HISTORY - 1) * sizeof(char *));
        history[MAX_HISTORY - 1] = cmd_copy;
    }
}

/* Check if a command is a built-in */
bool is_builtin(const char *command)
{
    for (int i = 0; command_table[i].name; i++)
    {
        if (!strcmp(command, command_table[i].name))
        {
            return true;
        }
    }
    return false;
}

/* Run a built-in command */
int run_builtin(char *args[])
{
    if (!args[0])
    {
        fprintf(stderr, "shush: no command provided\n");
        last_exit_status = 1;
        return 1;
    }
    for (int i = 0; command_table[i].name; i++)
    {
        if (!strcmp(args[0], command_table[i].name))
        {
            command_table[i].func(args);
            update_session(&session);
            return last_exit_status;
        }
    }
    fprintf(stderr, "shush: unknown built-in command: %s\n", args[0]);
    last_exit_status = 1;
    return 1;
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
        {
            putchar(' ');
        }
    }

    if (newline)
    {
        putchar('\n');
    }
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
            {
                free(history[i]);
            }
            history_count = 0;
            last_exit_status = 0;
        }
        else if (!strcmp(args[1], "-d") && args[2])
        {
            char *endptr;
            long index = strtol(args[2], &endptr, 10);
            if (*endptr || index < 1 || index > history_count)
            {
                fprintf(stderr, "history: %s: invalid index\n", args[2]);
                last_exit_status = 1;
                return;
            }
            index--; /* Convert to 0-based */
            free(history[index]);
            memmove(&history[index], &history[index + 1],
                    (history_count - index - 1) * sizeof(char *));
            history_count--;
            last_exit_status = 0;
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
        {
            printf("%d %s\n", i + 1, history[i]);
        }
        last_exit_status = 0;
    }
}

/* Built-in cd command */
void builtin_cd(char *args[])
{
    char cwd[PATH_MAX];
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

    if (!getcwd(cwd, sizeof(cwd)))
    {
        perror("getcwd");
        last_exit_status = 1;
        return;
    }

    char *oldpwd = getenv("PWD");
    if (oldpwd && setenv("OLDPWD", oldpwd, 1))
    {
        perror("setenv");
        last_exit_status = 1;
        return;
    }
    if (setenv("PWD", cwd, 1))
    {
        perror("setenv");
        last_exit_status = 1;
        return;
    }

    last_exit_status = 0;
}

/* Built-in ver command */
void builtin_ver(char *args[])
{
    char version[32];

#ifdef VERSION_STRING
    /*  Use custom version string if defined */
    snprintf(version, sizeof(version), "shush version %s\n", VERSION_STRING);
    printf("%s", version);
#else
    const char *month_str = __DATE__; /*  e.g., "Apr 30 2025" */
    const char *time_str = __TIME__;  /*  e.g., "00:50:12" */
    int year = 0, day = 0, hour = 0, minute = 0;
    char month[4] = {0}; /*  Initialize to avoid garbage */

    int parsed = sscanf(month_str, "%s %d %d", month, &day, &year);

    if (parsed != 3)
    {
        printf("shush version unknown\n");
        last_exit_status = 1;
        return;
    }

    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char *month_pos = strstr(months, month);
    int month_num = (month_pos ? (month_pos - months) / 3 + 1 : 1);

    parsed = sscanf(time_str, "%d:%d", &hour, &minute);

    /*  Check parsing success */
    if (parsed != 2)
    {
        printf("shush version unknown\n");
        last_exit_status = 1;
        return;
    }

    snprintf(version, sizeof(version), "development-%04d%02d%02d%02d%02d", year,
             month_num, day, hour, minute);
    printf("shush version %s\n", version);
#endif

    last_exit_status = 0;
}

/* Built-in exit command */
void builtin_exit(char *args[])
{
    int status = last_exit_status;
    if (args[1]) {
        char *endptr;
        status = strtol(args[1], &endptr, 10);
        if (*endptr) {
            fprintf(stderr, "exit: %s: numeric argument required\n", args[1]);
            last_exit_status = 1;
            status = 1;
        }
    }
    last_exit_status = status; /* Set before exiting */
    exit(status);
}

/* Built-in pwd command */
void builtin_pwd(char *args[])
{
    if (args[1] && strcmp(args[1], "-P") != 0 && strcmp(args[1], "-L") != 0)
    {
        fprintf(stderr, "pwd: invalid option -- '%s'\n", args[1]);
        last_exit_status = 1;
        return;
    }
    int logical = !args[1] || strcmp(args[1], "-L") == 0;
    if (logical)
    {
        char *pwd = getenv("PWD");
        if (pwd)
        {
            printf("%s\n", pwd);
            last_exit_status = 0;
        }
        else
        {
            perror("pwd");
            last_exit_status = 1;
        }
    }
    else
    {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)))
        {
            printf("%s\n", cwd);
            last_exit_status = 0;
        }
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
        fprintf(stderr, "set: invalid usage\n");
        last_exit_status = 1;
        return;
    }
    extern char **environ;
    for (char **env = environ; *env; ++env)
    {
        printf("%s\n", *env);
    }
    last_exit_status = 0;
}

/* Built-in unset command */
void builtin_unset(char *args[])
{
    int success = 1;
    for (int i = 1; args[i]; i++)
    {
        if (unsetenv(args[i]))
        {
            fprintf(stderr, "unset: %s: cannot unset\n", args[i]);
            success = 0;
        }
    }
    last_exit_status = success ? 0 : 1;
}

/* Built-in export command */
void builtin_export(char *args[])
{
    int success = 1;
    for (int i = 1; args[i]; i++)
    {
        char *eq = strchr(args[i], '=');
        if (eq)
        {
            *eq = '\0';
            if (setenv(args[i], eq + 1, 1))
            {
                fprintf(stderr, "export: %s: export failed\n", args[i]);
                success = 0;
            }
            *eq = '=';
        }
        else
        {
            fprintf(stderr, "export: %s: invalid format\n", args[i]);
            success = 0;
        }
    }
    last_exit_status = success ? 0 : 1;
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

    char *endptr;
    long pid = strtol(args[1], &endptr, 10);
    if (*endptr || pid <= 0)
    {
        fprintf(stderr, "kill: %s: invalid process ID\n", args[1]);
        last_exit_status = 1;
        return;
    }

    int sig = SIGTERM; /* Default signal */
    if (args[2])
    {
        int signum = sig_from_name(args[2]);
        if (signum != -1)
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

    if (kill((pid_t)pid, sig) == -1)
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
void builtin_alias(char *args[]) {
    if (!args[1]) {
        for (int i = 0; i < alias_count; i++) {
            printf("%s='%s'\n", aliases[i].name, aliases[i].value);
        }
        last_exit_status = 0;
        return;
    }
    if (!strcmp(args[1], "-d") && args[2]) {
        for (int i = 0; i < alias_count; i++) {
            if (!strcmp(aliases[i].name, args[2])) {
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
        return;
    }

    char *name = NULL;
    char *value = NULL;
    if (args[1] && strchr(args[1], '=')) {
        name = strdup(args[1]);
        if (!name) {
            fprintf(stderr, "alias: memory allocation failed\n");
            last_exit_status = 1;
            return;
        }
        char *eq = strchr(name, '=');
        *eq = '\0';
        if (eq[1] != '\0') {
            value = strdup(eq + 1);
        } else if (args[2]) {
            value = strdup(args[2]);
        }
    } else if (args[1] && args[2]) {
        name = strdup(args[1]);
        value = strdup(args[2]);
    } else {
        fprintf(stderr, "alias: missing value for '%s'\n", args[1]);
        free(name);
        last_exit_status = 1;
        return;
    }

    if (!name || !value) {
        fprintf(stderr, "alias: memory allocation failed\n");
        free(name);
        free(value);
        last_exit_status = 1;
        return;
    }

    size_t len = strlen(value);
    if (len >= 2 && value[0] == '\'' && value[len - 1] == '\'') {
        value[len - 1] = '\0';
        memmove(value, value + 1, len - 1);
    }

    if (alias_count >= MAX_ALIASES) {
        fprintf(stderr, "alias: too many aliases\n");
        free(name);
        free(value);
        last_exit_status = 1;
        return;
    }

    aliases[alias_count++] = (alias_t){name, value};
    last_exit_status = 0;
}

/* Built-in unalias command */
void builtin_unalias(char *args[])
{
    if (!args[1])
    {
        fprintf(stderr, "unalias: missing argument\n");
        last_exit_status = 1;
        return;
    }
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

/* Built-in source command */
void builtin_source(char *args[])
{
    if (!args[1]) {
        fprintf(stderr, "source: file not specified\n");
        last_exit_status = 1;
        return;
    }
    FILE *file = fopen(args[1], "r");
    if (!file) {
        perror("source");
        last_exit_status = 1;
        return;
    }
    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, file) != -1) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0]) {
            parse_and_execute(line);
        }
    }
    free(line);
    fclose(file);
}

/* Built-in jobs command */
void builtin_jobs(char *args[])
{
    extern pid_t bg_procs[];
    extern int bg_proc_count;

    if (args[1])
    {
        fprintf(stderr, "jobs: no arguments expected\n");
        last_exit_status = 1;
        return;
    }

    for (int i = 0; i < bg_proc_count; i++)
    {
        int status;
        pid_t result = waitpid(bg_procs[i], &status, WNOHANG);
        if (result == 0)
        {
            printf("[%d] Running %d\n", i + 1, bg_procs[i]);
        }
    }
    last_exit_status = 0;
}

/* Built-in fg command */
void builtin_fg(char *args[])
{
    extern pid_t bg_procs[];
    extern int bg_proc_count;

    if (!args[1])
    {
        if (bg_proc_count == 0)
        {
            fprintf(stderr, "fg: no current job\n");
            last_exit_status = 1;
            return;
        }
        args[1] = "1";
    }

    char *endptr;
    long job_id = strtol(args[1], &endptr, 10);
    if (*endptr || job_id < 1 || job_id > bg_proc_count)
    {
        fprintf(stderr, "fg: %s: invalid job ID\n", args[1]);
        last_exit_status = 1;
        return;
    }

    pid_t pid = bg_procs[job_id - 1];
    kill(pid, SIGCONT);
    int status;
    waitpid(pid, &status, 0);
    memmove(&bg_procs[job_id - 1], &bg_procs[job_id], (bg_proc_count - job_id) * sizeof(pid_t));
    bg_proc_count--;
    last_exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

/* Built-in bg command */
void builtin_bg(char *args[])
{
    extern pid_t bg_procs[];
    extern int bg_proc_count;

    if (!args[1])
    {
        if (bg_proc_count == 0)
        {
            fprintf(stderr, "bg: no current job\n");
            last_exit_status = 1;
            return;
        }
        args[1] = "1";
    }

    char *endptr;
    long job_id = strtol(args[1], &endptr, 10);
    if (*endptr || job_id < 1 || job_id > bg_proc_count)
    {
        fprintf(stderr, "bg: %s: invalid job ID\n", args[1]);
        last_exit_status = 1;
        return;
    }

    pid_t pid = bg_procs[job_id - 1];
    kill(pid, SIGCONT);
    printf("[%ld] %d continued\n", job_id, pid);
    last_exit_status = 0;
}

/* Custom completion for built-ins */
char **builtin_completion(const char *command, const char *word, size_t *count)
{
    char **candidates = malloc(MAX_ALIASES * sizeof(char *));
    if (!candidates)
    {
        *count = 0;
        return NULL;
    }
    *count = 0;

    if (!strcmp(command, "alias") || !strcmp(command, "unalias"))
    {
        size_t word_len = strlen(word);
        for (int i = 0; i < alias_count && *count < MAX_ALIASES; i++)
        {
            if (strncmp(aliases[i].name, word, word_len) == 0)
            {
                candidates[*count] = strdup(aliases[i].name);
                if (candidates[*count])
                {
                    (*count)++;
                }
            }
        }
    }
    else if (!strcmp(command, "kill"))
    {
        DIR *dir = opendir("/proc");
        if (dir)
        {
            struct dirent *entry;
            size_t word_len = strlen(word);
            while ((entry = readdir(dir)) && *count < MAX_ALIASES)
            {
                if (entry->d_type == DT_DIR && isdigit(entry->d_name[0]) &&
                    strncmp(entry->d_name, word, word_len) == 0)
                {
                    candidates[*count] = strdup(entry->d_name);
                    if (candidates[*count])
                    {
                        (*count)++;
                    }
                }
            }
            closedir(dir);
        }
    }

    if (*count == 0)
    {
        free(candidates);
        return NULL;
    }
    return candidates;
}

/* Look up an alias by name and return its value, or NULL if not found */
const char *lookup_alias(const char *name) {
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            return aliases[i].value;
        }
    }
    return NULL;
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
    {"jobs", builtin_jobs},
    {"fg", builtin_fg},
    {"bg", builtin_bg},
    {NULL, NULL}
};
