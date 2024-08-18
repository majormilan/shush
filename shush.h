#ifndef SHUSH_H
#define SHUSH_H

#include <stdbool.h>

// Declare global variables
extern char *home_directory;
extern int last_exit_status;

// Declare functions
void print_prompt();
void initialize_shell();

#endif // SHUSH_H
