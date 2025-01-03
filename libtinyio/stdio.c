#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include "stdio.h"

// Helper function to print an integer
void tiny_print_int(int n) {
    if (n == 0) {
        tiny_putchar('0');
        return;
    }
    if (n < 0) {
        tiny_putchar('-');
        n = -n;
    }
    char buffer[10];
    int i = 0;
    while (n != 0) {
        buffer[i++] = (n % 10) + '0';
        n /= 10;
    }
    while (i > 0) {
        tiny_putchar(buffer[--i]);
    }
}

// Helper function to print a string
void tiny_print_string(const char *str) {
    while (*str) {
        tiny_putchar(*str++);
    }
}

// Custom printf function
int tiny_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int count = tiny_vprintf(format, args);
    va_end(args);
    return count;
}

int tiny_vprintf(const char *format, va_list args) {
    int count = 0;
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd': {
                    int int_arg = va_arg(args, int);
                    tiny_print_int(int_arg);
                    break;
                }
                case 's': {
                    const char *str_arg = va_arg(args, const char *);
                    tiny_print_string(str_arg);
                    break;
                }
                case 'c': {
                    int char_arg = va_arg(args, int);
                    tiny_putchar(char_arg);
                    break;
                }
                default:
                    tiny_putchar('%');
                    tiny_putchar(*format);
                    break;
            }
        } else {
            tiny_putchar(*format);
        }
        format++;
        count++;
    }
    return count;
}

// Custom fprintf function
int tiny_fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int count = tiny_vfprintf(stream, format, args);
    va_end(args);
    return count;
}

int tiny_vfprintf(FILE *stream, const char *format, va_list args) {
    int count = 0;
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd': {
                    int int_arg = va_arg(args, int);
                    char buffer[12];
                    snprintf(buffer, 12, "%d", int_arg);
                    fputs(buffer, stream);
                    count += strlen(buffer);
                    break;
                }
                case 's': {
                    const char *str_arg = va_arg(args, const char *);
                    fputs(str_arg, stream);
                    count += strlen(str_arg);
                    break;
                }
                case 'c': {
                    int char_arg = va_arg(args, int);
                    fputc(char_arg, stream);
                    count++;
                    break;
                }
                default:
                    fputc('%', stream);
                    fputc(*format, stream);
                    count += 2;
                    break;
            }
        } else {
            fputc(*format, stream);
            count++;
        }
        format++;
    }
    return count;
}

// Custom snprintf function
int tiny_snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int count = tiny_vsnprintf(str, size, format, args);
    va_end(args);
    return count;
}

int tiny_vsnprintf(char *str, size_t size, const char *format, va_list args) {
    size_t count = 0;
    while (*format && count < size - 1) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd': {
                    int int_arg = va_arg(args, int);
                    int written = snprintf(str, size - count, "%d", int_arg);
                    str += written;
                    count += written;
                    break;
                }
                case 's': {
                    const char *str_arg = va_arg(args, const char *);
                    while (*str_arg && count < size - 1) {
                        *str++ = *str_arg++;
                        count++;
                    }
                    break;
                }
                case 'c': {
                    int char_arg = va_arg(args, int);
                    if (count < size - 1) {
                        *str++ = char_arg;
                        count++;
                    }
                    break;
                }
                default:
                    if (count < size - 1) {
                        *str++ = '%';
                        count++;
                    }
                    if (count < size - 1) {
                        *str++ = *format;
                        count++;
                    }
                    break;
            }
        } else {
            *str++ = *format;
            count++;
        }
        format++;
    }
    *str = '\0';
    return count;
}

// Custom scanf function
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

// Custom putchar function
int tiny_putchar(int c) {
    return write(STDOUT_FILENO, &c, 1);
}
