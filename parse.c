/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Parse and execute commands for Simple Humane Shell (shush).
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "parse.h"

#define MAX_LINE 80

static int execute_command(char *cmd);
static char *trim_whitespace(char *str);

bool
is_directory(const char *path)
{
	struct stat path_stat;

	return (stat(path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode));
}

static int
execute_command(char *cmd)
{
	char *args[MAX_LINE / 2 + 1];
	char *token = strtok(cmd, " \t\r\n\a");
	int i = 0;

	while (token) {
		args[i++] = token;
		token = strtok(NULL, " \t\r\n\a");
	}
	args[i] = NULL;

	if (!args[0])
		return 0;

	if (strcmp(args[0], "exit") == 0)
		exit(0);

	add_to_history(args[0]);

	if (is_builtin(args[0])) {
		run_builtin(args);
		return last_exit_status;
	}

	pid_t pid = fork();
	if (pid == 0) {
		execvp(args[0], args);
		perror("shush");
		exit(EXIT_FAILURE);
	} else if (pid < 0) {
		perror("shush: fork failed");
		return -1;
	} else {
		int status;
		waitpid(pid, &status, 0);
		last_exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
		return last_exit_status;
	}
}

static char *
trim_whitespace(char *str)
{
	char *end;

	while (isspace((unsigned char)*str))
		str++;

	if (!*str)
		return str;

	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;

	end[1] = '\0';

	return str;
}

void
parse_and_execute(char *line)
{
	char *cmd_start = line;
	char *cmd_end;
	int status = 0;
	bool execute_next = true;

	while (*cmd_start) {
		cmd_start = trim_whitespace(cmd_start);
		if (!*cmd_start)
			break;

		cmd_end = cmd_start;
		while (*cmd_end && *cmd_end != ';' && *cmd_end != '&' && *cmd_end != '|') {
			if (*cmd_end == '"') {
				cmd_end++;
				while (*cmd_end && *cmd_end != '"')
					cmd_end++;
				if (*cmd_end)
					cmd_end++;
			} else {
				cmd_end++;
			}
		}

		char operator = *cmd_end;
		if (*cmd_end) {
			*cmd_end = '\0';
			cmd_end++;
		}

		char *cmd = trim_whitespace(cmd_start);

		if (execute_next && *cmd) {
			char *expanded_command = expand_variables(cmd);
			if (is_directory(expanded_command)) {
				printf("shush: %s: Is a directory\n", expanded_command);
			} else {
				status = execute_command(expanded_command);
			}
			free(expanded_command);
		}

		if (operator == '&') {
			if (*cmd_end == '&') {
				execute_next = (status == 0);
				cmd_end++;
			} else {
				execute_next = true;
			}
		} else if (operator == '|') {
			if (*cmd_end == '|') {
				execute_next = (status != 0);
				cmd_end++;
			} else {
				execute_next = true;
			}
		} else if (operator == ';') {
			execute_next = true;
		} else {
			execute_next = true;
		}

		cmd_start = cmd_end;
	}
}

char *
expand_variables(const char *input)
{
	if (!home_directory) {
		fprintf(stderr, "Error: HOME is not set\n");
		exit(EXIT_FAILURE);
	}

	size_t len = strlen(input);
	char *expanded = malloc(len + 1);
	if (!expanded) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	strcpy(expanded, input);

	char *result = malloc(len + 1);
	if (!result) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	size_t result_len = 0;

	for (size_t i = 0; i < len; i++) {
		if (expanded[i] == '~') {
			size_t home_len = strlen(home_directory);
			result = realloc(result, result_len + home_len + 1);
			if (!result) {
				perror("realloc");
				exit(EXIT_FAILURE);
			}
			strcpy(result + result_len, home_directory);
			result_len += home_len;
		} else if (expanded[i] == '$' && i + 1 < len) {
			char *env_var_name = expanded + i + 1;
			char *end = env_var_name;

			while (*end && (isalnum(*end) || *end == '_'))
				end++;

			size_t var_name_len = end - env_var_name;
			if (var_name_len > 0) {
				char var_name[var_name_len + 1];
				strncpy(var_name, env_var_name, var_name_len);
				var_name[var_name_len] = '\0';

				char *var_value = getenv(var_name);
				if (var_value) {
					size_t value_len = strlen(var_value);
					result = realloc(result, result_len + value_len + 1);
					if (!result) {
						perror("realloc");
						exit(EXIT_FAILURE);
					}
					strcpy(result + result_len, var_value);
					result_len += value_len;
				}
				i += var_name_len;
			}
		} else {
			result = realloc(result, result_len + 2);
			if (!result) {
				perror("realloc");
				exit(EXIT_FAILURE);
			}
			result[result_len++] = expanded[i];
		}
	}

	result[result_len] = '\0';
	free(expanded);
	return result;
}
