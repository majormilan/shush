/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 */

#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdbool.h>

#include "init.h"

typedef struct {
    const char *name;
    void (*func)(char *[]);
} builtin_command;

extern char *home_directory;
extern int last_exit_status;

bool is_builtin(const char *command);
void run_builtin(char *args[]);

void builtin_cd(char *args[]);
void builtin_echo(char *args[]);
void builtin_env(char *args[]);
void builtin_history(char *args[]);
void builtin_kill(char *args[]);
void builtin_ls(char *args[]);
void builtin_pwd(char *args[]);
void builtin_set(char *args[]);
void builtin_source(char *args[]);
void builtin_unset(char *args[]);
void builtin_ver(char *args[]);

void add_to_history(const char *command);

extern const builtin_command command_table[];

#endif /* BUILTINS_H */
