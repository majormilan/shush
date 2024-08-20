/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 */

#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>

void parse_and_execute(char *line);
char *expand_variables(const char *input);

#endif /* PARSE_H */
