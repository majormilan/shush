#include "stdio.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>

/*  Define standard file pointers for custom FILE struct */
FILE _stdin = {
    .fd = STDIN_FILENO, .error = 0, .eof = 0, .ungetc_buf = 0, .has_ungetc = 0};
FILE _stdout = {.fd = STDOUT_FILENO,
                .error = 0,
                .eof = 0,
                .ungetc_buf = 0,
                .has_ungetc = 0};
FILE _stderr = {.fd = STDERR_FILENO,
                .error = 0,
                .eof = 0,
                .ungetc_buf = 0,
                .has_ungetc = 0};
FILE *stdin = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

/* Helper function to print an integer into a buffer */
int tiny_print_int_to_buffer(char *buffer, size_t size, int n)
{
    if (size == 0)
        return 0;

    char temp[10];
    int i = 0, count = 0, is_negative = 0;

    if (n < 0)
    {
        is_negative = 1;
        n = -n;
    }
    else if (n == 0)
    {
        if (count < size - 1)
            buffer[count++] = '0';
        buffer[count] = '\0';
        return count;
    }

    while (n != 0)
    {
        temp[i++] = (n % 10) + '0';
        n /= 10;
    }

    if (is_negative)
        temp[i++] = '-';

    while (i > 0 && count < size - 1)
    {
        buffer[count++] = temp[--i];
    }

    buffer[count] = '\0';
    return count;
}

/* Custom snprintf function */
int tiny_snprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int count = tiny_vsnprintf(str, size, format, args);
    va_end(args);
    return count;
}

int tiny_vsnprintf(char *str, size_t size, const char *format, va_list args)
{
    size_t count = 0;
    while (*format && count < size - 1)
    {
        if (*format == '%')
        {
            format++;
            switch (*format)
            {
                case 'd':
                {
                    int int_arg = va_arg(args, int);
                    int written =
                        tiny_print_int_to_buffer(str, size - count, int_arg);
                    str += written;
                    count += written;
                    break;
                }
                case 's':
                {
                    const char *str_arg = va_arg(args, const char *);
                    while (*str_arg && count < size - 1)
                    {
                        *str++ = *str_arg++;
                        count++;
                    }
                    break;
                }
                case 'c':
                {
                    int char_arg = va_arg(args, int);
                    if (count < size - 1)
                    {
                        *str++ = char_arg;
                        count++;
                    }
                    break;
                }
                default:
                    if (count < size - 1)
                    {
                        *str++ = '%';
                        count++;
                    }
                    if (count < size - 1)
                    {
                        *str++ = *format;
                        count++;
                    }
                    break;
            }
        }
        else
        {
            *str++ = *format;
            count++;
        }
        format++;
    }
    *str = '\0';
    return count;
}

/* Custom fputc function */
int tiny_fputc(int c, FILE *stream)
{
    return write(stream->fd, &c, 1) == 1 ? c : EOF;
}

/* Custom fputs function */
int tiny_fputs(const char *str, FILE *stream)
{
    size_t len = 0;
    const char *s = str;

    while (*s++)
        len++;

    return write(stream->fd, str, len) == len ? 0 : EOF;
}

/* Custom fgetc function */
int tiny_fgetc(FILE *stream)
{
    char c;
    if (stream->has_ungetc)
    {
        stream->has_ungetc = 0;
        return (unsigned char)stream->ungetc_buf;
    }
    if (read(stream->fd, &c, 1) == 1)
    {
        return (unsigned char)c;
    }
    else
    {
        stream->eof = 1;
        return EOF;
    }
}

/* Custom fgets function */
char *tiny_fgets(char *str, int n, FILE *stream)
{
    if (n <= 0)
        return NULL;

    char *ptr = str;
    while (n > 1)
    {
        int c = tiny_fgetc(stream);
        if (c == EOF)
            break;
        *ptr++ = (char)c;
        if (c == '\n')
            break;
        n--;
    }
    *ptr = '\0';

    return (ptr == str) ? NULL : str;
}

/* Function to write ANSI escape sequences */
void tiny_set_color(const char *color_code, FILE *stream)
{
    tiny_fputs(color_code, stream);
}

/* Function to reset text formatting */
void tiny_reset_color(FILE *stream) { tiny_fputs(ANSI_RESET, stream); }

/* Helper function to print an integer */
void tiny_print_int(int n)
{
    if (n == 0)
    {
        tiny_putchar('0');
        return;
    }
    if (n < 0)
    {
        tiny_putchar('-');
        n = -n;
    }
    char buffer[10];
    int i = 0;
    while (n != 0)
    {
        buffer[i++] = (n % 10) + '0';
        n /= 10;
    }
    while (i > 0)
    {
        tiny_putchar(buffer[--i]);
    }
}

/* Helper function to print a string */
void tiny_print_string(const char *str)
{
    while (*str)
    {
        tiny_putchar(*str++);
    }
}

/* Custom printf function */
int tiny_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int count = tiny_vprintf(format, args);
    va_end(args);
    return count;
}

int tiny_vprintf(const char *format, va_list args)
{
    return tiny_vfprintf(stdout, format, args);
}

/* Custom fprintf function */
int tiny_fprintf(FILE *stream, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int count = tiny_vfprintf(stream, format, args);
    va_end(args);
    return count;
}

int tiny_vfprintf(FILE *stream, const char *format, va_list args)
{
    int count = 0;
    while (*format)
    {
        if (*format == '%')
        {
            format++;
            switch (*format)
            {
                case 'd':
                {
                    int int_arg = va_arg(args, int);
                    char buffer[12];
                    tiny_snprintf(buffer, 12, "%d", int_arg);
                    tiny_fputs(buffer, stream);
                    count += strlen(buffer);
                    break;
                }
                case 's':
                {
                    const char *str_arg = va_arg(args, const char *);
                    tiny_fputs(str_arg, stream);
                    count += strlen(str_arg);
                    break;
                }
                case 'c':
                {
                    int char_arg = va_arg(args, int);
                    tiny_fputc(char_arg, stream);
                    count++;
                    break;
                }
                case 'C': /*  Custom case for setting text color */
                {
                    const char *color_code = va_arg(args, const char *);
                    tiny_set_color(color_code, stream);
                    break;
                }
                case 'R': /*  Custom case for resetting text formatting */
                {
                    tiny_reset_color(stream);
                    break;
                }
                default:
                    tiny_fputc('%', stream);
                    tiny_fputc(*format, stream);
                    count += 2;
                    break;
            }
        }
        else
        {
            tiny_fputc(*format, stream);
            count++;
        }
        format++;
    }
    return count;
}

/* Custom scanf function */
int tiny_scanf(const char *input, const char *format, int *arg1, int *arg2)
{
    const char *p = format;
    int *int_ptr;
    char *char_ptr;
    int num_parsed;
    int arg_index = 0;

    while (*p != '\0')
    {
        if (*p == '%')
        {
            p++;
            if (*p == 'd')
            {
                int_ptr = (arg_index == 0) ? arg1 : arg2;
                num_parsed = 0;
                while (*input >= '0' && *input <= '9')
                {
                    num_parsed = num_parsed * 10 + (*input - '0');
                    input++;
                }
                *int_ptr = num_parsed;
                arg_index++;
            }
            else if (*p == 's')
            {
                char_ptr = (char *)((arg_index == 0) ? arg1 : arg2);
                while (*input != '\0' && *input != ' ' && *input != '\n')
                {
                    *char_ptr = *input;
                    char_ptr++;
                    input++;
                }
                *char_ptr = '\0';
                arg_index++;
            }
        }
        else
        {
            if (*input != *p)
            {
                return -1; /* Format mismatch */
            }
            input++;
        }
        p++;
    }

    return 0; /* Success */
}

/* Custom putchar function */
int tiny_putchar(int c) { return write(STDOUT_FILENO, &c, 1); }

/* Custom getchar function */
int tiny_getchar(void)
{
    char c;
    return read(STDIN_FILENO, &c, 1) == 1 ? (unsigned char)c : EOF;
}

/* Custom fflush function */
int tiny_fflush(FILE *stream) { return fsync(stream->fd); }

/* Custom fileno function */
int tiny_fileno(FILE *stream) { return stream->fd; }

/* Custom fgetc function */
/* int tiny_fgetc(FILE *stream)
{
    char c;
    if (stream->has_ungetc) {
        stream->has_ungetc = 0;
        return (unsigned char)stream->ungetc_buf;
    }
    if (read(stream->fd, &c, 1) == 1) {
        return (unsigned char)c;
    } else {
        stream->eof = 1;
        return EOF;
    }
}*/

/* Custom ungetc function */
int tiny_ungetc(int c, FILE *stream)
{
    if (c == EOF || stream->has_ungetc)
    {
        return EOF;
    }
    stream->ungetc_buf = (char)c;
    stream->has_ungetc = 1;
    return c;
}

/* Custom feof function */
int tiny_feof(FILE *stream) { return stream->eof; }

/* Custom ferror function */
int tiny_ferror(FILE *stream) { return stream->error; }

/* Custom clearerr function */
void tiny_clearerr(FILE *stream)
{
    stream->error = 0;
    stream->eof = 0;
}

/* Custom perror function */
void tiny_perror(const char *s)
{
    if (s && *s)
    {
        tiny_fprintf(stderr, "%s: %s\n", s, strerror(errno));
    }
    else
    {
        tiny_fprintf(stderr, "%s\n", strerror(errno));
    }
}

/* Custom fopen function */
FILE *tiny_fopen(const char *filename, const char *mode)
{
    int fd = -1;
    if (strcmp(mode, "r") == 0)
    {
        fd = open(filename, O_RDONLY);
    }
    else if (strcmp(mode, "w") == 0)
    {
        fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    }
    else if (strcmp(mode, "a") == 0)
    {
        fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0666);
    }
    else if (strcmp(mode, "r+") == 0)
    {
        fd = open(filename, O_RDWR);
    }
    else if (strcmp(mode, "w+") == 0)
    {
        fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    }
    else if (strcmp(mode, "a+") == 0)
    {
        fd = open(filename, O_RDWR | O_CREAT | O_APPEND, 0666);
    }

    if (fd == -1)
    {
        return NULL;
    }

    FILE *file = (FILE *)malloc(sizeof(FILE));
    if (!file)
    {
        close(fd);
        return NULL;
    }

    file->fd = fd;
    file->error = 0;
    file->eof = 0;
    file->ungetc_buf = 0;
    file->has_ungetc = 0;

    return file;
}

/* Custom fclose function */
int tiny_fclose(FILE *stream)
{
    int result = close(stream->fd);
    free(stream);
    return result;
}

/* Custom freopen function */
FILE *tiny_freopen(const char *filename, const char *mode, FILE *stream)
{
    if (stream)
    {
        tiny_fclose(stream);
    }
    return tiny_fopen(filename, mode);
}
