#ifndef COMPLETION_H
#define COMPLETION_H

#include <stddef.h>
#include <termios.h>

/* Completion callback type, includes command for context */
typedef char **(*readline_completion_cb)(const char *command, const char *word, const char *buffer, size_t *count);

/* Set the completion callback */
void completion_set_callback(readline_completion_cb callback);

/* Handle tab completion */
void completion_complete(const char *prompt, char *buffer, size_t *len,
                  size_t *cursor_pos, size_t prompt_width, int prompt_row,
                  int prompt_col, struct termios *orig_termios);

#endif /* COMPLETION_H */
