#include "readline.h"
#include "completion.h"
#include "../libtinyio/stdio.h"
#include "../libtinyio/string.h"
#include "config.h"
#include "utf8.h"
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* Calculate the display width of a UTF-8 string */
size_t utf8_string_width(const char *str) {
    size_t width = 0;
    while (*str) {
        int char_width = utf8_char_width(str);
        width += char_width;
        str = utf8_next(str);
    }
    return width;
}

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

static int get_terminal_width() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return 80; // Default width on error
    }
    return ws.ws_col;
}

void redraw_line(const char *prompt, const char *buffer, size_t cursor_pos,
                 size_t prompt_width, int prompt_row, int prompt_col)
{
    int term_width = get_terminal_width();

    // Move cursor to start of the prompt and clear everything below it.
    move_cursor_to_position(prompt_row, prompt_col);
    printf("\033[J"); // Clear from cursor to end of screen

    // Print the prompt and buffer
    printf("%s%s", prompt, buffer);

    // Calculate the new cursor position, accounting for wrapping
    size_t visual_cursor_pos_in_buffer = 0;
    const char* p = buffer;
    for (size_t i = 0; i < cursor_pos; ) {
        size_t char_len = utf8_char_length(p);
        visual_cursor_pos_in_buffer += utf8_char_width(p);
        p += char_len;
        i += char_len;
    }

    size_t total_visual_pos = prompt_width + visual_cursor_pos_in_buffer;
    int new_row = prompt_row + (prompt_col - 1 + total_visual_pos) / term_width;
    int new_col = (prompt_col - 1 + total_visual_pos) % term_width + 1;

    // Move the hardware cursor to its new calculated position
    move_cursor_to_position(new_row, new_col);
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
    size_t prompt_width = utf8_string_width(prompt); /* Use display width instead of strlen */

    /* Validate prompt to ensure it's valid UTF-8 */
    if (!utf8_validate(prompt)) {
        fprintf(stderr, "Invalid UTF-8 in prompt\n");
        free(buffer);
        disable_raw_mode(&orig_termios);
        return NULL;
    }

    int prompt_row, prompt_col;
    get_cursor_position(&prompt_row, &prompt_col);

    redraw_line(prompt, buffer, cursor_pos, prompt_width, prompt_row, prompt_col);

    while (1)
    {
        c = getchar();
        if (c == '\n' || c == EOF)
        {
            buffer[len] = '\0';
            break;
        }
        else if (c == 9)
        {
            completion_complete(prompt, buffer, &len, &cursor_pos, prompt_width,
                         prompt_row, prompt_col, &orig_termios);
        }
        else if (c == 127 || c == 8)
        {
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
                    redraw_line(prompt, buffer, cursor_pos, prompt_width,
                                prompt_row, prompt_col);
                }
            }
        }
        else if (c == 27)
        {
            getchar();
            c = getchar();
            if (c == '3')
            {
                getchar();
                if (cursor_pos < len)
                {
                    delete_char_at_cursor(&cursor_pos, &len, buffer);
                    redraw_line(prompt, buffer, cursor_pos, prompt_width,
                                prompt_row, prompt_col);
                }
            }
            else if (c == 'C')
            {
                const char *next_char = utf8_next(buffer + cursor_pos);
                if (next_char && cursor_pos < len)
                {
                    cursor_pos = next_char - buffer;
                    redraw_line(prompt, buffer, cursor_pos, prompt_width,
                                prompt_row, prompt_col);
                }
            }
            else if (c == 'D')
            {
                const char *prev_char = utf8_prev(buffer, buffer + cursor_pos);
                if (prev_char)
                {
                    cursor_pos = prev_char - buffer;
                    redraw_line(prompt, buffer, cursor_pos, prompt_width,
                                prompt_row, prompt_col);
                }
            }
            else if (c == 'H')
            {
                cursor_pos = 0;
                redraw_line(prompt, buffer, cursor_pos, prompt_width, prompt_row,
                            prompt_col);
            }
            else if (c == 'F')
            {
                cursor_pos = len;
                redraw_line(prompt, buffer, cursor_pos, prompt_width, prompt_row,
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
                redraw_line(prompt, buffer, cursor_pos, prompt_width, prompt_row,
                            prompt_col);
            }
        }
    }

    printf("\n");

    disable_raw_mode(&orig_termios);
    return buffer;
}