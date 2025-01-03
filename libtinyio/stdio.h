#ifndef TINY_STDIO_H
#define TINY_STDIO_H

#include <stdarg.h>
#include <stdio.h>

// Function declarations
int tiny_printf(const char *format, ...);
int tiny_vprintf(const char *format, va_list args);
int tiny_fprintf(FILE *stream, const char *format, ...);
int tiny_vfprintf(FILE *stream, const char *format, va_list args);
int tiny_snprintf(char *str, size_t size, const char *format, ...);
int tiny_vsnprintf(char *str, size_t size, const char *format, va_list args);
int tiny_scanf(const char *input, const char *format, int *arg1, int *arg2);
int tiny_putchar(int c);

// Redefine standard functions to use the tiny versions
#define printf tiny_printf
#define fprintf tiny_fprintf
#define snprintf tiny_snprintf

#endif // TINY_STDIO_H
