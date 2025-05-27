#include "string.h"
#include "signal.h"
#include <errno.h>
#include <stdlib.h>

void *tiny_memcpy(void *dest, const void *src, size_t n)
{
    char *d = dest;
    const char *s = src;
    while (n--)
    {
        *d++ = *s++;
    }
    return dest;
}

void *tiny_memmove(void *dest, const void *src, size_t n)
{
    char *d = dest;
    const char *s = src;
    if (d < s)
    {
        while (n--)
        {
            *d++ = *s++;
        }
    }
    else
    {
        const char *lasts = s + (n - 1);
        char *lastd = d + (n - 1);
        while (n--)
        {
            *lastd-- = *lasts--;
        }
    }
    return dest;
}

char *tiny_strdup(const char *s)
{
    size_t len = tiny_strlen(s) + 1;
    char *copy = malloc(len);
    if (copy)
    {
        tiny_memcpy(copy, s, len);
    }
    return copy;
}

char *tiny_strndup(const char *s, size_t n)
{
    char *copy = malloc(n + 1);
    if (copy)
    {
        tiny_strncpy(copy, s, n);
        copy[n] = '\0';
    }
    return copy;
}

char *tiny_strcat(char *dest, const char *src)
{
    char *d = dest;
    while (*d)
    {
        d++;
    }
    while ((*d++ = *src++))
    {
        ;
    }
    return dest;
}

int tiny_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

size_t tiny_strlen(const char *s)
{
    const char *p = s;
    while (*p)
    {
        p++;
    }
    return p - s;
}

int tiny_strncmp(const char *s1, const char *s2, size_t n)
{
    while (n && *s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
    {
        return 0;
    }
    else
    {
        return *(unsigned char *)s1 - *(unsigned char *)s2;
    }
}

char *tiny_strchr(const char *s, int c)
{
    while (*s != (char)c)
    {
        if (*s == '\0')
        {
            return NULL;
        }
        s++;
    }
    return (char *)s;
}

char *tiny_strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++))
    {
        ;
    }
    return dest;
}

char *tiny_strncpy(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (n && (*d++ = *src++))
    {
        n--;
    }
    while (n--)
    {
        *d++ = '\0';
    }
    return dest;
}

static char *strtok_save = NULL;

char *tiny_strtok(char *str, const char *delim)
{
    if (str)
    {
        strtok_save = str;
    }
    else if (!strtok_save)
    {
        return NULL;
    }

    str = strtok_save + tiny_strspn(strtok_save, delim);
    strtok_save = str + tiny_strcspn(str, delim);

    if (str == strtok_save)
    {
        return strtok_save = NULL;
    }

    if (*strtok_save)
    {
        *strtok_save++ = '\0';
    }
    else
    {
        strtok_save = NULL;
    }

    return str;
}

char *tiny_strerror(int errnum)
{
    switch (errnum)
    {
        case EINVAL:
            return "Invalid argument";
        case EIO:
            return "Input/output error";
        case ENOENT:
            return "No such file or directory";
        /*  Add more cases as needed */
        default:
            return "Unknown error";
    }
}

size_t tiny_strspn(const char *s, const char *accept)
{
    const char *p;
    const char *a;
    size_t count = 0;

    for (p = s; *p != '\0'; ++p)
    {
        for (a = accept; *a != '\0'; ++a)
        {
            if (*p == *a)
            {
                break;
            }
        }
        if (*a == '\0')
        {
            return count;
        }
        ++count;
    }

    return count;
}

char *tiny_strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    while (*haystack) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (char *)haystack;
        haystack++;
    }
    return NULL;
}

size_t tiny_strcspn(const char *s, const char *reject) {
    const char *p;
    const char *r;
    size_t count = 0;

    for (p = s; *p != '\0'; ++p) {
        for (r = reject; *r != '\0'; ++r) {
            if (*p == *r) {
                return count;
            }
        }
        ++count;
    }
    return count;
}
