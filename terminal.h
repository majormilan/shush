/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Header file for terminal-related functions.
 */

#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h>  // Include this header for size_t

void update_prompt(char *prompt, size_t size);
char *terminal_readline(const char *prompt);
#endif /* TERMINAL_H */
