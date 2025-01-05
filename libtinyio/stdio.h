#ifndef TINY_STDIO_H
#define TINY_STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <unistd.h>

/*  ANSI escape sequences */
#define ANSI_RESET "\x1b[0m"
#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_BLUE "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN "\x1b[36m"
#define ANSI_WHITE "\x1b[37m"

#define EOF -1

/*  Undefine existing definitions to avoid conflicts */
#ifdef FILE
#undef FILE
#endif
#ifdef stdin
#undef stdin
#endif
#ifdef stdout
#undef stdout
#endif
#ifdef stderr
#undef stderr
#endif

/*  Custom FILE struct */
typedef struct
{
    int fd;
    int error;
    int eof;
    char ungetc_buf;
    int has_ungetc;
} FILE;

/*  Define standard file pointers */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/*  Function declarations */
int tiny_printf(const char *format, ...);
int tiny_vprintf(const char *format, va_list args);
int tiny_fprintf(FILE *stream, const char *format, ...);
int tiny_vfprintf(FILE *stream, const char *format, va_list args);
int tiny_snprintf(char *str, size_t size, const char *format, ...);
int tiny_vsnprintf(char *str, size_t size, const char *format, va_list args);
int tiny_scanf(const char *input, const char *format, int *arg1, int *arg2);
int tiny_putchar(int c);
int tiny_fputc(int c, FILE *stream);
int tiny_fputs(const char *str, FILE *stream);
int tiny_getchar(void);
int tiny_fflush(FILE *stream);
int tiny_fileno(FILE *stream);
char *tiny_fgets(char *str, int n, FILE *stream);
int tiny_fgetc(FILE *stream); /*  Declaration of tiny_fgetc */
int tiny_feof(FILE *stream);
int tiny_ferror(FILE *stream);
void tiny_clearerr(FILE *stream);
void tiny_perror(const char *s);
FILE *tiny_fopen(const char *filename, const char *mode);
int tiny_fclose(FILE *stream);
FILE *tiny_freopen(const char *filename, const char *mode, FILE *stream);
int tiny_ungetc(int c, FILE *stream);

/*  ANSI escape sequence functions */
void tiny_set_color(const char *color_code, FILE *stream);
void tiny_reset_color(FILE *stream);

/*  Redefine standard functions to use the tiny versions */
#define printf tiny_printf
#define fprintf tiny_fprintf
#define snprintf tiny_snprintf
#define fputc tiny_fputc
#define fputs tiny_fputs
#define getchar tiny_getchar
#define putchar tiny_putchar
#define fflush tiny_fflush
#define fileno tiny_fileno
#define fgets tiny_fgets
#define feof tiny_feof
#define ferror tiny_ferror
#define clearerr tiny_clearerr
#define perror tiny_perror
#define fopen tiny_fopen
#define fclose tiny_fclose
#define freopen tiny_freopen
#define ungetc tiny_ungetc

#endif /*  TINY_STDIO_H */
