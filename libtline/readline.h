#ifndef READLINE_H
#define READLINE_H

#include "completion.h"
#include <termios.h>

char *readline(const char *prompt);

/* Internal functions for tab completion */
void disable_raw_mode(struct termios *orig_termios);
void enable_raw_mode(struct termios *orig_termios);
void move_cursor_to_position(int row, int col);
void redraw_line(const char *prompt, const char *buffer, size_t cursor_pos,
                 size_t prompt_width, int prompt_row, int prompt_col);

#endif /* READLINE_H */
