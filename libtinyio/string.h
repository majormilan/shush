#ifndef TINY_STRING_H
#define TINY_STRING_H

typedef unsigned long size_t; /* Basic size_t definition for portability */

char *tiny_strndup(const char *s, size_t n);
int tiny_strcmp(const char *s1, const char *s2);
size_t tiny_strlen(const char *s);
char *tiny_strcpy(char *dest, const char *src);
char *tiny_strncpy(char *dest, const char *src, size_t n);

#define strndup tiny_strndup
#define strcmp tiny_strcmp
#define strlen tiny_strlen
#define strcpy tiny_strcpy
#define strncpy tiny_strncpy

#endif /* TINY_STRING_H */
