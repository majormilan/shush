/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Terminal-related functions for reading input and handling command-line interface.
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "terminal.h"

/* Macros */
#define MAX_INPUT_LENGTH  8192
#define MAX_SUGGESTIONS   256

/* Function declarations */
static void move_cursor(int offset);
static void backspace(size_t *pos, char *buffer, size_t *buffer_size);
static int list_commands(const char *prefix, char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH]);
static int list_files(const char *path, const char *prefix, char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH]);
static int get_terminal_width(void);
static void print_suggestions_grid(char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH], int suggestion_count);
static int is_path(const char *buffer);
static int complete_path(const char *buffer, char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH]);
static void tab_complete(const char *prompt, char *buffer, size_t *pos, size_t *buffer_size);

/* Global variables */

/* Function definitions */

/*
 * Update the shell prompt with user, hostname, and current directory.
 */
void
update_prompt(char *prompt, size_t size)
{
	char cwd[1024];
	char temp[1024];
	const char *home = getenv("HOME");
	const char *user = getenv("USER") ? "user" : getenv("USER");
	const char *hostname = getenv("HOSTNAME") ? "localhost" : getenv("HOSTNAME");

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
 * Move cursor by a specified offset (positive for right, negative for left).
 */
static void
move_cursor(int offset)
{
	if (offset != 0) {
		printf("\033[%d%c", abs(offset), offset > 0 ? 'C' : 'D');
		fflush(stdout);
	}
}

/*
 * Handle backspace key by removing a character from the input buffer.
 */
static void
backspace(size_t *pos, char *buffer, size_t *buffer_size)
{
	if (*pos == 0)
		return;

	printf("\b \b");
	(*pos)--;
	(*buffer_size)--;
	memmove(buffer + *pos, buffer + *pos + 1, *buffer_size - *pos + 1);

	printf("%s", buffer + *pos);
	size_t end_len = *buffer_size - *pos;

	if (end_len > 0) {
		printf("%*s\b", (int)end_len + 1, " ");
		move_cursor(-(int)end_len);
	}
	fflush(stdout);
}

/*
 * List possible command completions based on a given prefix.
 */
static int
list_commands(const char *prefix, char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH])
{
	char *path = getenv("PATH");
	char *dir = strtok(path, ":");
	int suggestion_count = 0;

	while (dir && suggestion_count < MAX_SUGGESTIONS) {
		DIR *dp = opendir(dir);
		if (dp) {
			struct dirent *entry;
			while ((entry = readdir(dp)) != NULL && suggestion_count < MAX_SUGGESTIONS) {
				if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0)
					strncpy(suggestions[suggestion_count++], entry->d_name, MAX_INPUT_LENGTH);
			}
			closedir(dp);
		}
		dir = strtok(NULL, ":");
	}
	return suggestion_count;
}

/*
 * List files in a directory that match the given prefix.
 */
static int
list_files(const char *path, const char *prefix, char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH])
{
	DIR *dir = opendir(path);
	if (!dir)
		return 0;

	struct dirent *entry;
	int suggestion_count = 0;

	while ((entry = readdir(dir)) != NULL && suggestion_count < MAX_SUGGESTIONS) {
		if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0)
			strncpy(suggestions[suggestion_count++], entry->d_name, MAX_INPUT_LENGTH);
	}
	closedir(dir);
	return suggestion_count;
}

/*
 * Get the current terminal width.
 */
static int
get_terminal_width(void)
{
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	return w.ws_col;
}

/*
 * Print a list of suggestions in a grid format.
 */
static void
print_suggestions_grid(char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH], int suggestion_count)
{
	int i, len, max_len = 0;
	int term_width, cols;

	if (suggestion_count == 0)
		return;

	for (i = 0; i < suggestion_count; i++) {
		len = strlen(suggestions[i]);
		if (len > max_len)
			max_len = len;
	}
	max_len += 2;

	term_width = get_terminal_width();
	cols = term_width / max_len;

	for (i = 0; i < suggestion_count; i++) {
		printf("%-*s", max_len, suggestions[i]);
		if ((i + 1) % cols == 0 || i == suggestion_count - 1)
			printf("\n");
	}
}

/*
 * Check if the input buffer contains a path.
 */
static int
is_path(const char *buffer)
{
	return (buffer[0] == '.' || buffer[0] == '/');
}

/*
 * Handle path completion based on the buffer contents.
 */
static int
complete_path(const char *buffer, char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH])
{
	char *last_slash = strrchr(buffer, '/');

	if (last_slash) {
		*last_slash = '\0';
		int result = list_files(buffer, last_slash + 1, suggestions);
		*last_slash = '/';
		return result;
	}
	return list_files(".", buffer, suggestions);
}

/*
 * Perform tab completion for commands and file paths.
 */
static void
tab_complete(const char *prompt, char *buffer, size_t *pos, size_t *buffer_size)
{
	char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH];
	int suggestion_count = 0;

	if (*buffer_size == 0)
		return;

	if (is_path(buffer)) {
		suggestion_count = complete_path(buffer, suggestions);
	} else {
		char *first_space = strchr(buffer, ' ');
		if (first_space == NULL) {
			suggestion_count = list_commands(buffer, suggestions);
		} else {
			suggestion_count = complete_path(first_space + 1, suggestions);
		}
	}

	if (suggestion_count == 1) {
		size_t suggestion_len = strlen(suggestions[0]);
		strncpy(buffer + *pos, suggestions[0] + (*pos ? strlen(buffer) : 0), suggestion_len);
		*pos += suggestion_len;
		*buffer_size = *pos;
		buffer[*buffer_size] = '\0';
		printf("%s", buffer + *pos - suggestion_len);
	} else if (suggestion_count > 1) {
		printf("\n");
		print_suggestions_grid(suggestions, suggestion_count);
		printf("%s%s", prompt, buffer);
		move_cursor(-(int)(*buffer_size - *pos));
	}
	fflush(stdout);
}

/*
 * Read user input from the terminal, including handling special keys.
 */
char *
terminal_readline(const char *prompt)
{
	char buffer[MAX_INPUT_LENGTH];
	struct termios oldt, newt;
	int ch;
	size_t pos = 0, buffer_size = 0;

	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	printf("%s", prompt);
	fflush(stdout);

	while ((ch = getchar()) != EOF && ch != '\n') {
		if (ch == 27) {
			getchar();
			switch (getchar()) {
			case 'C':
				if (pos < buffer_size)
					move_cursor(1), pos++;
				break;
			case 'D':
				if (pos > 0)
					move_cursor(-1), pos--;
				break;
			}
			continue;
		}
		if (ch == '\t') {
			tab_complete(prompt, buffer, &pos, &buffer_size);
			continue;
		}
		if (ch == 127) {
			backspace(&pos, buffer, &buffer_size);
			continue;
		}
		if (isprint(ch)) {
			memmove(buffer + pos + 1, buffer + pos, buffer_size - pos);
			buffer[pos++] = ch;
			buffer_size++;
			buffer[buffer_size] = '\0';

			printf("%s", buffer + pos - 1);
			move_cursor(-(int)(buffer_size - pos));
			fflush(stdout);
		}
	}

	buffer[buffer_size] = '\0';
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	printf("\n");

	if (buffer_size == 0)
		return NULL;

	return strdup(buffer);
}

/*
 * Main function: provide terminal-like input system with tab completion.
 */
int
read_multiline_input(void)
{
	char *input;
	char prompt[1024];

	while (1) {
		update_prompt(prompt, sizeof(prompt));
		input = terminal_readline(prompt);

		if (input == NULL || strcmp(input, "exit") == 0) {
			free(input);
			break;
		}

		system(input);
		free(input);
	}
	return 0;
}
