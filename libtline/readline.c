#include "readline.h"
#include "../libtinyio/stdio.h"
#include "../libtinyio/string.h"
#include "config.h"
#include "utf8.h"
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

void disable_raw_mode(struct termios *orig_termios)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig_termios);
}

void enable_raw_mode(struct termios *orig_termios)
{
    struct termios raw = *orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void get_cursor_position(int *rows, int *cols)
{
    char buf[32];
    unsigned int i = 0;
    printf("\033[6n");
    fflush(stdout);
    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, buf + i, 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\033' || buf[1] != '[')
        return;
    tiny_scanf(buf + 2, "%d;%d", rows, cols);
}

void move_cursor_to_position(int row, int col)
{
    printf("\033[%d;%dH", row, col);
}

void redraw_line(const char *prompt, const char *buffer, size_t cursor_pos,
                 size_t prompt_len, int prompt_row, int prompt_col)
{
    move_cursor_to_position(prompt_row, prompt_col);
    printf("\033[K"); /* Clear from cursor to end of line */
    printf("%s", prompt);
    printf("%s", buffer);

    size_t visual_cursor_pos = 0;
    for (size_t i = 0; i < cursor_pos;)
    {
        size_t char_len = utf8_char_length(buffer + i);
        visual_cursor_pos++;
        i += char_len;
    }
    move_cursor_to_position(prompt_row,
                            prompt_col + prompt_len + visual_cursor_pos);
    fflush(stdout);
}

static void delete_char_at_cursor(size_t *cursor_pos, size_t *len, char *buffer)
{
    if (*cursor_pos < *len)
    {
        const char *next_char = utf8_next(buffer + *cursor_pos);
        if (next_char)
        {
            size_t utf8_char_len = next_char - (buffer + *cursor_pos);
            memmove(buffer + *cursor_pos, buffer + *cursor_pos + utf8_char_len,
                    *len - *cursor_pos - utf8_char_len);
            *len -= utf8_char_len;
            buffer[*len] = '\0';
        }
    }
}

char *readline(const char *prompt)
{
    struct termios orig_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    enable_raw_mode(&orig_termios);

    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer)
    {
        perror("Unable to allocate buffer");
        exit(EXIT_FAILURE);
    }

    size_t len = 0;
    size_t cursor_pos = 0;
    char input_buffer[4];
    int c;
    size_t prompt_len = strlen(prompt);

    int prompt_row, prompt_col;
    get_cursor_position(&prompt_row, &prompt_col);

    redraw_line(prompt, buffer, cursor_pos, prompt_len, prompt_row, prompt_col);

    while (1)
    {
        c = getchar();
        if (c == '\n' || c == EOF)
        {
            buffer[len] = '\0';
            break;
        }
        else if (c == 9)
        { /* TAB key */
            tab_complete(prompt, buffer, &len, &cursor_pos, prompt_len,
                         prompt_row, prompt_col, &orig_termios);
        }
        else if (c == 127)
        { /* BACKSPACE */
            if (cursor_pos > 0)
            {
                const char *prev_char = utf8_prev(buffer, buffer + cursor_pos);
                if (prev_char)
                {
                    size_t utf8_char_len = buffer + cursor_pos - prev_char;
                    cursor_pos -= utf8_char_len;
                    len -= utf8_char_len;
                    memmove(buffer + cursor_pos,
                            buffer + cursor_pos + utf8_char_len,
                            len - cursor_pos);
                    buffer[len] = '\0';
                    redraw_line(prompt, buffer, cursor_pos, prompt_len,
                                prompt_row, prompt_col);
                }
            }
        }
        else if (c == 27)
        { /* Escape sequence */
            getchar();
            c = getchar();
            if (c == '3')
            { /* DELETE key */
                getchar();
                if (cursor_pos < len)
                {
                    delete_char_at_cursor(&cursor_pos, &len, buffer);
                    redraw_line(prompt, buffer, cursor_pos, prompt_len,
                                prompt_row, prompt_col);
                }
            }
            else if (c == 'C')
            { /* Right arrow */
                const char *next_char = utf8_next(buffer + cursor_pos);
                if (next_char && cursor_pos < len)
                {
                    cursor_pos = next_char - buffer;
                    redraw_line(prompt, buffer, cursor_pos, prompt_len,
                                prompt_row, prompt_col);
                }
            }
            else if (c == 'D')
            { /* Left arrow */
                const char *prev_char = utf8_prev(buffer, buffer + cursor_pos);
                if (prev_char)
                {
                    cursor_pos = prev_char - buffer;
                    redraw_line(prompt, buffer, cursor_pos, prompt_len,
                                prompt_row, prompt_col);
                }
            }
            else if (c == 'H')
            { /* HOME key */
                cursor_pos = 0;
                redraw_line(prompt, buffer, cursor_pos, prompt_len, prompt_row,
                            prompt_col);
            }
            else if (c == 'F')
            { /* END key */
                cursor_pos = len;
                redraw_line(prompt, buffer, cursor_pos, prompt_len, prompt_row,
                            prompt_col);
            }
        }
        else
        {
            input_buffer[0] = (char)c;
            int char_len = utf8_char_length(input_buffer);

            for (int i = 1; i < char_len; ++i)
            {
                input_buffer[i] = getchar();
            }

            if (len + char_len < BUFFER_SIZE - 1)
            {
                if (cursor_pos < len)
                {
                    memmove(buffer + cursor_pos + char_len, buffer + cursor_pos,
                            len - cursor_pos);
                }
                memcpy(buffer + cursor_pos, input_buffer, char_len);
                cursor_pos += char_len;
                len += char_len;
                redraw_line(prompt, buffer, cursor_pos, prompt_len, prompt_row,
                            prompt_col);
            }
        }
    }

    printf("\n");

    disable_raw_mode(&orig_termios);
    return buffer;
}
