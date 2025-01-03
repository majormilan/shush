#include "utf8.h"

int utf8_validate(const char *str)
{
    const unsigned char *s = (const unsigned char *)str;
    while (*s)
    {
        int len = utf8_char_length((const char *)s);
        if (len == 0 || (len == 1 && (*s & 0x80)))
            return 0; /*  Invalid leading byte */
        for (int i = 1; i < len; ++i)
        {
            if ((s[i] & 0xC0) != 0x80)
                return 0; /*  Check continuation bytes */
        }
        s += len;
    }
    return 1;
}

const char *utf8_next(const char *str)
{
    int len = utf8_char_length(str);
    if (len == 0)
        return NULL; /*  Invalid UTF-8 */
    return str + len;
}

const char *utf8_prev(const char *start, const char *current)
{
    const char *s = current - 1;
    while (s >= start)
    {
        if ((*s & 0xC0) != 0x80)
            return s; /*  Find the start of the UTF-8 character */
        --s;
    }
    return NULL; /*  No valid character found */
}

int utf8_char_length(const char *str)
{
    unsigned char c = (unsigned char)*str;
    int retVal = 0;
    if (c < 0x80)
        retVal = 1;
    if ((c & 0xE0) == 0xC0)
        retVal = 2;
    if ((c & 0xF0) == 0xE0)
        retVal = 3;
    if ((c & 0xF8) == 0xF0)
        retVal = 4;
    return retVal;
}

int utf8_char_width(const char *str)
{
    int codepoint = 0;
    int len = utf8_char_length(str);
    if (len == 1)
    {
        codepoint = (unsigned char)str[0];
    }
    else
    {
        codepoint = str[0] & ((1 << (8 - len)) - 1);
        for (int i = 1; i < len; ++i)
        {
            codepoint = (codepoint << 6) | (str[i] & 0x3F);
        }
    }

    /*  Basic width logic; expand for specific needs */
    if (codepoint >= 0x1100 && codepoint <= 0x115F)
        return 2; /*  Example: Wide characters */
    if (codepoint >= 0x1F300 && codepoint <= 0x1F64F)
        return 2; /*  Emojis */
    return 1;
}
