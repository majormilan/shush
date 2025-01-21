#ifndef STRING_H
#define STRING_H

#include <stddef.h>

void *tiny_memcpy(void *dest, const void *src, size_t n);
void *tiny_memmove(void *dest, const void *src, size_t n);
char *tiny_strdup(const char *s);
char *tiny_strndup(const char *s, size_t n);
char *tiny_strcat(char *dest, const char *src);
int tiny_strcmp(const char *s1, const char *s2);
size_t tiny_strlen(const char *s);
int tiny_strncmp(const char *s1, const char *s2, size_t n);
char *tiny_strchr(const char *s, int c);
char *tiny_strcpy(char *dest, const char *src);
char *tiny_strncpy(char *dest, const char *src, size_t n);
char *tiny_strtok(char *str, const char *delim);
char *tiny_strerror(int errnum);
const char *tiny_strsignal(int sig);
size_t tiny_strspn(const char *s, const char *accept);
size_t tiny_strcspn(const char *s, const char *reject);

#define memcpy tiny_memcpy
#define memmove tiny_memmove
#define strdup tiny_strdup
#define strndup tiny_strndup
#define strcat tiny_strcat
#define strcmp tiny_strcmp
#define strlen tiny_strlen
#define strncmp tiny_strncmp
#define strchr tiny_strchr
#define strcpy tiny_strcpy
#define strncpy tiny_strncpy
#define strtok tiny_strtok

#ifdef strerror
#undef strerror
#endif
#define strerror tiny_strerror

#define strsignal tiny_strsignal
#define strspn tiny_strspn
#define strcspn tiny_strcspn

#endif // STRING_H
