#ifndef UTF8_H
#define UTF8_H

#include <stddef.h>

// Validation
int utf8_validate(const char *str);

// Navigation
const char *utf8_next(const char *str);
const char *utf8_prev(const char *start, const char *current);

// Character Metrics
int utf8_char_length(const char *str); // Byte length of a UTF-8 character
int utf8_char_width(const char *str);  // Display width of a UTF-8 character

#endif
