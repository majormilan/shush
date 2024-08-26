/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Built-in commands for Simple Humane Shell (shush).
 */

#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdbool.h>

extern char *home_directory;
extern int last_exit_status;

/* Built-in command structure */
typedef struct {
	const char *name;
	void (*func)(char *args[]);
} builtin_command_t;

/* Built-in command table */
extern const builtin_command_t command_table[];

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

#endif /* BUILTINS_H */

