#include "readline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

static void disable_raw_mode(struct termios* orig_termios) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig_termios);
}

static void enable_raw_mode(struct termios* orig_termios) {
    struct termios raw = *orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void clear_line() {
    printf("\033[K");
    fflush(stdout);
}

static void move_cursor(int offset) {
    printf("\033[%dC", offset);
    fflush(stdout);
}

static void move_cursor_left(int offset) {
    printf("\033[%dD", offset);
    fflush(stdout);
}

static void insert_char_at_cursor(char c) {
    printf("%c", c);
    fflush(stdout);
}

static void delete_char_at_cursor(size_t *cursor_pos, size_t *len, char *buffer) {
    if (*cursor_pos < *len) {
        memmove(buffer + *cursor_pos, buffer + *cursor_pos + 1, *len - *cursor_pos);
        (*len)--;
        printf("\033[1D");
        clear_line();
        printf("%s", buffer + *cursor_pos);
        move_cursor_left(*len - *cursor_pos);
    }
}

char* readline(const char* prompt) {
    printf("%s", prompt);
    fflush(stdout);  // Ensure the prompt is displayed before we start reading input

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
    int c;
    while (1) {
        c = getchar();
        if (c == '\n' || c == EOF) {
            buffer[len] = '\0';
            break;
        } else if (c == 127) { // Handle backspace
            if (cursor_pos > 0) {
                cursor_pos--;
                len--;
                memmove(buffer + cursor_pos, buffer + cursor_pos + 1, len - cursor_pos);
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c == 27) { // Handle escape sequences
            getchar(); // Skip '['
            c = getchar();
            if (c == '3') { // Handle delete key
                getchar(); // Skip '~'
                delete_char_at_cursor(&cursor_pos, &len, buffer);
            } else if (c == 'C') { // Right arrow key
                if (cursor_pos < len) {
                    cursor_pos++;
                    move_cursor(1);
                }
            } else if (c == 'D') { // Left arrow key
                if (cursor_pos > 0) {
                    cursor_pos--;
                    move_cursor_left(1);
                }
            }
        } else {
            if (len < BUFFER_SIZE - 1) {
                if (cursor_pos < len) {
                    memmove(buffer + cursor_pos + 1, buffer + cursor_pos, len - cursor_pos);
                }
                buffer[cursor_pos] = c;
                cursor_pos++;
                len++;
                printf("%c", c);
                fflush(stdout);
            }
        }
    }

    // Move cursor to the end of input line
    printf("\n");

    disable_raw_mode(&orig_termios);
    return buffer;
}
