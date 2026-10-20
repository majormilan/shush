#ifndef COMPLETION_H
#define COMPLETION_H

#include <stddef.h>
#include <termios.h>

/* Handle tab completion */
void completion_complete(const char *prompt, char *buffer, size_t *len,
                  size_t *cursor_pos, size_t prompt_width, int prompt_row,
                  int prompt_col, struct termios *orig_termios);

#endif /* COMPLETION_H */
