/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Parsing and executing commands for Simple Humane Shell (shush).
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include "parse.h"
#include "builtins.h"

#define MAX_LINE 1024

static bool debug = false;

/* Function Prototypes */
static int exec_cmd(char *cmd);
static char *trim(char *str);
static char *parse_arg(char **cmd);
static void handle_chain(char *line);
static void handle_result(char sep, int status, bool *exec_next);
static size_t append_env_var(char **res, size_t *res_len, const char *env_name);
static int exec_external(char *args[]);

/* Custom strndup implementation */
static char *
strndup(const char *s, size_t n)
{
    char *p;
    size_t len = strnlen(s, n);
    p = malloc(len + 1);
    if (p) {
        memcpy(p, s, len);
        p[len] = '\0';
    }
    return p;
}

void
set_debug(bool mode)
{
	debug = mode;
}

void
parse_and_execute(char *line)
{
	handle_chain(line);
}

static void
handle_chain(char *line)
{
	char *cmd, *end;
	int status = 0;
	bool exec_next = true;

	while (*line) {
		line = trim(line);
		if (!*line)
			break;

		end = line + strcspn(line, ";&|");

		if (exec_next) {
			cmd = strndup(line, end - line);
			if (!cmd) {
				perror("strndup");
				exit(1);
			}
			char *expanded = expand_variables(cmd); /* Reverting to original name */
			free(cmd);

			if (!expanded) {
				fprintf(stderr, "Failed to expand command\n");
				exit(1);
			}

			status = exec_cmd(expanded);
			free(expanded);
		}

		handle_result(*end, status, &exec_next);
		line = *end ? end + 1 : end;
	}
}

static void
handle_result(char sep, int status, bool *exec_next)
{
	switch (sep) {
	case '&':
		*exec_next = (status == 0);
		break;
	case '|':
		*exec_next = (status != 0);
		break;
	default:
		*exec_next = true;
		break;
	}
}

static int
exec_cmd(char *cmd)
{
	char *args[MAX_LINE / 2 + 1];
	int i = 0;

	while (*cmd) {
		cmd = trim(cmd);
		if (!*cmd)
			break;

		args[i++] = parse_arg(&cmd);
		if (!args[i - 1]) {
			fprintf(stderr, "Failed to parse argument\n");
			return -1;
		}
	}
	args[i] = NULL;

	if (!args[0])
		return 0;

	if (debug) {
		printf("Executing: %s\n", args[0]);
		for (int j = 0; j < i; j++)
			printf("arg[%d]: %s\n", j, args[j]);
	}

	if (!strcmp(args[0], "exit"))
		exit(0);

	add_to_history(args[0]); /* Reverting to original name */

	if (is_builtin(args[0])) {
		run_builtin(args);
		return last_exit_status;
	}

	return exec_external(args); /* Ensure this function matches the declaration */
}

static int
exec_external(char *args[])
{
	pid_t pid = fork();

	if (pid == 0) {
		execvp(args[0], args);
		perror("shush");
		exit(1);
	} else if (pid < 0) {
		perror("shush: fork failed");
		return -1;
	} else {
		int status;
		waitpid(pid, &status, 0);
		return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
	}
}

static char *
trim(char *str)
{
	while (isspace((unsigned char)*str))
		str++;
	if (*str == 0)
		return str;

	char *end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;

	end[1] = '\0';
	return str;
}

static char *
parse_arg(char **cmd)
{
	char *str = *cmd, *start = str;
	bool quoted = false;

	while (*str && (quoted || !isspace((unsigned char)*str))) {
		if (*str == '"')
			quoted = !quoted;
		else if (*str == '\\' && *(str + 1))
			str++;
		str++;
	}

	char *tok = strndup(start, str - start);
	if (!tok) {
		perror("strndup");
		exit(1);
	}

	*cmd = *str ? str + 1 : str;
	return tok;
}

char *
expand_variables(const char *input) /* Reverting to original name */
{
	if (!home_directory) {
		fprintf(stderr, "Error: HOME not set\n");
		exit(1);
	}

	size_t len = strlen(input);
	char *res = malloc(len + 1);
	if (!res) {
		perror("malloc");
		exit(1);
	}

	size_t res_len = 0;
	for (size_t i = 0; i < len; i++) {
		if (input[i] == '~') {
			size_t home_len = strlen(home_directory);
			res = realloc(res, res_len + home_len + 1);
			if (!res) {
				perror("realloc");
				exit(1);
			}
			strcpy(res + res_len, home_directory);
			res_len += home_len;
		} else if (input[i] == '$' && i + 1 < len) {
			i += append_env_var(&res, &res_len, input + i + 1);
		} else {
			res[res_len++] = input[i];
		}
	}

	res[res_len] = '\0';
	return res;
}

static size_t
append_env_var(char **res, size_t *res_len, const char *env_name)
{
	const char *end = env_name;

	while (*end && (isalnum(*end) || *end == '_'))
		end++;

	size_t name_len = end - env_name;
	if (name_len) {
		char var[name_len + 1];
		strncpy(var, env_name, name_len);
		var[name_len] = '\0';

		char *val = getenv(var);
		if (val) {
			size_t val_len = strlen(val);
			*res = realloc(*res, *res_len + val_len + 1);
			if (!*res) {
				perror("realloc");
				exit(1);
			}
			strcpy(*res + *res_len, val);
			*res_len += val_len;
		}
	}
	return name_len;
}
