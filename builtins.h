#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdbool.h>

// Define the structure for built-in commands
typedef struct {
    const char *name;       // Name of the command
    void (*func)(char *[]); // Function pointer to the command's implementation
} builtin_command;

// History array and count
extern char *history[];
extern int history_count;

// Shell variables
extern char *home_directory;
extern int last_exit_status;

// Built-in commands
void builtin_echo(char *args[]);
void builtin_history(char *args[]);
void builtin_cd(char *args[]);
void builtin_ver(char *args[]);

// Function to add to history
void add_to_history(const char *command);

// Function to expand variables
char *expand_variables(const char *input);

#endif // BUILTINS_H

