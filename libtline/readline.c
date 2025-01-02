#include "readline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include "utf8.h"

#define BUFFER_SIZE 1024

static void disable_raw_mode(struct termios* orig_termios) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig_termios);
}

static void enable_raw_mode(struct termios* orig_termios) {
    struct termios raw = *orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void redraw_line(const char *prompt, const char *buffer, size_t cursor_pos, size_t prompt_len) {
    size_t buffer_len = strlen(buffer);
    size_t total_cursor_pos = prompt_len + buffer_len;
    printf("\033[%zuD", total_cursor_pos);
    printf("\033[K");
    printf("%s", prompt);
    printf("%s", buffer);
    fflush(stdout);
}


static void delete_char_at_cursor(size_t* cursor_pos, size_t* len, char* buffer) {
    if (*cursor_pos > 0) {
        const char* prev_char = utf8_prev(buffer, buffer + *cursor_pos);
        if (*len!=0) {
            size_t utf8_char_len = buffer + *cursor_pos - prev_char;
            *cursor_pos -= utf8_char_len;
            *len -= utf8_char_len;
            memmove(buffer + *cursor_pos, buffer + *cursor_pos + utf8_char_len, *len - *cursor_pos);
            buffer[*len] = '\0';
        }
    }
}

char* readline(const char* prompt) {
    struct termios orig_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    enable_raw_mode(&orig_termios);

    char* buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        perror("Unable to allocate buffer");
        exit(EXIT_FAILURE);
    }

    size_t len = 0;
    size_t cursor_pos = 0;
    char input_buffer[4];
    int c;
    size_t prompt_len = strlen(prompt);

    redraw_line(prompt, buffer, cursor_pos, prompt_len);

    while (1) {
        c = getchar();
        if (c == '\n' || c == EOF) {
            buffer[len] = '\0';
            break;
        } else if (c == 127) { // Backspace
            delete_char_at_cursor(&cursor_pos, &len, buffer);
            redraw_line(prompt, buffer, cursor_pos, prompt_len);
        } else if (c == 27) { // Escape sequence
            getchar();
            c = getchar();
            if (c == '3') { // Delete key
                getchar();
                delete_char_at_cursor(&cursor_pos, &len, buffer);
                redraw_line(prompt, buffer, cursor_pos, prompt_len);
            } else if (c == 'C') { // Right arrow
                const char *next_char = utf8_next(buffer + cursor_pos);
                if (next_char) {
                    cursor_pos = next_char - buffer;
                    redraw_line(prompt, buffer, cursor_pos, prompt_len);
                }
            } else if (c == 'D') { // Left arrow
                const char *prev_char = utf8_prev(buffer, buffer + cursor_pos);
                if (prev_char) {
                    cursor_pos = prev_char - buffer;
                    redraw_line(prompt, buffer, cursor_pos, prompt_len);
                }
            }
        } else {
            input_buffer[0] = (char)c;
            int char_len = utf8_char_length(input_buffer);

            for (int i = 1; i < char_len; ++i) {
                input_buffer[i] = getchar();
            }

            if (len + char_len < BUFFER_SIZE - 1) {
                if (cursor_pos < len) {
                    memmove(buffer + cursor_pos + char_len, buffer + cursor_pos, len - cursor_pos);
                }
                memcpy(buffer + cursor_pos, input_buffer, char_len);
                cursor_pos += char_len;
                len += char_len;
                redraw_line(prompt, buffer, cursor_pos, prompt_len);
            }
        }
    }

    printf("\n");

    disable_raw_mode(&orig_termios);
    return buffer;
}
