#include "terminal.h"
#include "libtline/readline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_INPUT_LENGTH  8192

// Update the prompt using existing code
void update_prompt(char *prompt, size_t size)
{
    char cwd[1024];
    char temp[1024];
    const char *home = getenv("HOME");
    const char *user = getenv("USER") ? getenv("USER") : "user";
    const char *hostname = getenv("HOSTNAME") ? getenv("HOSTNAME") : "localhost";

    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd");
        snprintf(cwd, sizeof(cwd), "[unknown]");
    } else if (home && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(temp, sizeof(temp), "~%s", cwd + strlen(home));
        strncpy(cwd, temp, sizeof(cwd));
    }

    if (geteuid() == 0)
        snprintf(prompt, size, "[%s@%s %s]# ", user, hostname, cwd);
    else
        snprintf(prompt, size, "[%s@%s %s]$ ", user, hostname, cwd);

    if (strlen(prompt) >= size) {
        fprintf(stderr, "Warning: Prompt string truncated.\n");
        prompt[size - 1] = '\0';
    }
}

/*
 * Use the readline function from libtline for input handling.
 */
char *terminal_readline(const char *prompt)
{
    // Use readline from libtline instead of custom implementation
    return readline(prompt);
}

