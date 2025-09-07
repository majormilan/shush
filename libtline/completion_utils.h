#ifndef COMPLETION_UTILS_H
#define COMPLETION_UTILS_H

#include <stddef.h>

void find_word_boundaries(const char *buffer, size_t cursor_pos,
                                 size_t *word_start, size_t *word_end);
char *extract_command(const char *buffer, size_t cursor_pos);
char *normalize_path(const char *path);
int is_valid_var_name(const char *str);
char *my_strsep(char **stringp, const char *delim);
char *get_home_dir_for_user(const char *username);
int compare_strings(const void *a, const void *b);

#endif /* COMPLETION_UTILS_H */
