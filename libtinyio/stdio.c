#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include "stdio.h"

int tiny_scanf(const char *input, const char *format, int *arg1, int *arg2) {
    const char *p = format;
    int *int_ptr;
    char *char_ptr;
    int num_parsed;
    int arg_index = 0;

    while (*p != '\0') {
        if (*p == '%') {
            p++;
            if (*p == 'd') {
                int_ptr = (arg_index == 0) ? arg1 : arg2;
                num_parsed = 0;
                while (*input >= '0' && *input <= '9') {
                    num_parsed = num_parsed * 10 + (*input - '0');
                    input++;
                }
                *int_ptr = num_parsed;
                arg_index++;
            } else if (*p == 's') {
                char_ptr = (char *)((arg_index == 0) ? arg1 : arg2);
                while (*input != '\0' && *input != ' ' && *input != '\n') {
                    *char_ptr = *input;
                    char_ptr++;
                    input++;
                }
                *char_ptr = '\0';
                arg_index++;
            }
        } else {
            if (*input != *p) {
                return -1; // Format mismatch
            }
            input++;
        }
        p++;
    }

    return 0; // Success
}


