#ifndef COMPLETION_CALLBACKS_H
#define COMPLETION_CALLBACKS_H

#include <stddef.h>

typedef char **(*completion_callback_func)(const char *command, const char *word, const char *buffer, size_t *count, char **path_prefix);

typedef struct {
    const char *command;
    completion_callback_func callback;
} CompletionCallback;

completion_callback_func get_completion_callback(const char *command);

#endif /* COMPLETION_CALLBACKS_H */
