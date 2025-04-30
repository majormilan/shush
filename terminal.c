/*
 * MIT/X Consortium License
 * Header file for terminal-related functions.
 */
#include "terminal.h"
#include "libtinyio/stdio.h"
#include "libtinyio/string.h"
#include "libtline/readline.h"
#include <stdlib.h>
#include <unistd.h>

#define MAX_INPUT_LENGTH 8192
#define MAX_PATH_LENGTH 1024

/* Update the shell prompt with the current user, hostname, and directory */
void update_prompt(char *prompt, size_t size)
{
    char cwd[MAX_PATH_LENGTH];
    char temp[MAX_PATH_LENGTH];
    const char *home = getenv("HOME");
    const char *user = getenv("USER") ? getenv("USER") : "user";
    const char *hostname =
        getenv("HOSTNAME") ? getenv("HOSTNAME") : "localhost";

    if (!getcwd(cwd, sizeof(cwd)))
    {
        perror("getcwd");
        snprintf(cwd, sizeof(cwd), "[unknown]");
    }
    else if (home && strncmp(cwd, home, strlen(home)) == 0)
    {
        snprintf(temp, sizeof(temp), "~%s", cwd + strlen(home));
        strncpy(cwd, temp, sizeof(cwd));
    }

    snprintf(prompt, size, "[%s@%s %s]%c ", user, hostname, cwd,
             geteuid() == 0 ? '#' : '$');

    if (strlen(prompt) >= size)
    {
        fprintf(stderr, "Warning: Prompt string truncated.\n");
        prompt[size - 1] = '\0';
    }
}

/* Use the readline function from libtline for input handling */
char *terminal_readline(const char *prompt) { return readline(prompt); }
