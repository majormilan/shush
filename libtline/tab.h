#ifndef TAB_H
#define TAB_H

#include <stddef.h>
#include <termios.h>

/* Completion callback type */
typedef char **(*readline_completion_cb)(const char *word, size_t *count);

/* Set the completion callback */
void tab_set_completion_callback(readline_completion_cb callback);

/* Handle tab completion */
void tab_complete(const char *prompt, char *buffer, size_t *len, size_t *cursor_pos,
                 size_t prompt_len, int prompt_row, int prompt_col,
                 struct termios *orig_termios);

#endif /* TAB_H */
