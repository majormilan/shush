#ifndef COMPLETION_GENERATORS_H
#define COMPLETION_GENERATORS_H

#include <stddef.h>

char **complete_environment_variables(const char *word, size_t *count, char **path_prefix);
char **complete_paths(const char *command, const char *word, size_t *count, char **path_prefix, int is_command);
char **complete_commands(const char *word, size_t *count, char **path_prefix);

#endif /* COMPLETION_GENERATORS_H */
