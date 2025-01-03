#ifndef TINY_STRING_H
#define TINY_STRING_H

typedef unsigned long size_t; /*  Basic size_t definition for portability */

char *tiny_strndup(const char *s, size_t n);

#define strndup tiny_strndup

#endif /*  TINY_STRING_H */
