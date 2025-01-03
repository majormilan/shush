#include <stddef.h>
/*  Provide minimal declarations for used functions */
void *malloc(size_t size);                           /*  Declaration for malloc */
void *memcpy(void *dest, const void *src, size_t n); /*  Declaration for memcpy */

/*  Custom strnlen implementation */
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
    p = (char *)malloc(len + 1); /*  Cast malloc to avoid warnings in strict C */
    if (p)
    {
        memcpy(p, s, len);
        p[len] = '\0';
    }
    return p;
}
