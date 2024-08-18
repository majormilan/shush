#ifndef SHUSH_H
#define SHUSH_H

#include <stdbool.h>

// Declare global variables
extern char *home_directory;
extern int last_exit_status;

// Declare functions
bool is_builtin(const char *command);
void run_builtin(char *args[]);
void parse_and_execute(char *line);

#endif // SHUSH_H
