#include <stddef.h>

/* Provide minimal declarations for used functions */
void *malloc(size_t size); /* Declaration for malloc */
void *memcpy(void *dest, const void *src,
             size_t n); /* Declaration for memcpy */

/* Custom strnlen implementation */
static size_t tiny_strnlen(const char *s, size_t maxlen)
{
    size_t len = 0;
    while (len < maxlen && s[len] != '\0')
    {
        len++;
    }
    return len;
}

/* Custom strndup implementation */
char *tiny_strndup(const char *s, size_t n)
{
    char *p;
    size_t len = tiny_strnlen(s, n);
    p = (char *)malloc(len + 1); /* Cast malloc to avoid warnings in strict C */
    if (p)
    {
        memcpy(p, s, len);
        p[len] = '\0';
    }
    return p;
}

/* Custom strcmp implementation */
int tiny_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/* Custom strlen implementation */
size_t tiny_strlen(const char *s)
{
    const char *sc = s;
    while (*sc != '\0')
    {
        sc++;
    }
    return sc - s;
}

/* Custom strcpy implementation */
char *tiny_strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++) != '\0')
        ;
    return dest;
}

/* Custom strncpy implementation */
char *tiny_strncpy(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (n && (*d++ = *src++) != '\0')
    {
        n--;
    }
    if (n)
    {
        while (--n)
        {
            *d++ = '\0';
        }
    }
    return dest;
}
