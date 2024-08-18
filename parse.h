#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>

// Function to parse and execute commands
void parse_and_execute(char *line);

// Function to expand variables in the command
char *expand_variables(const char *input);

#endif // PARSE_H
