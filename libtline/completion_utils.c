#include "completion_utils.h"
#include "../libtinyio/stdio.h"
#include "../libtinyio/string.h"
#include "config.h"
#include "utf8.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Compare function for sorting candidates */
int compare_strings(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

void find_word_boundaries(const char *buffer, size_t cursor_pos,
                                 size_t *word_start, size_t *word_end)
{
    *word_end = cursor_pos;
    *word_start = cursor_pos;

    /* Move backward to find word start, stopping at whitespace */
    while (*word_start > 0 && !isspace(buffer[*word_start - 1]))
    {
        const char *prev = utf8_prev(buffer, buffer + *word_start);
        if (!prev)
            break;
        *word_start = prev - buffer;
    }

    /* Include '$' for environment variables if followed by valid char */
    if (*word_start > 0 && buffer[*word_start] == '$' && *word_start > 1 &&
        !isspace(buffer[*word_start - 1]))
    {
        const char *prev = utf8_prev(buffer, buffer + *word_start);
        if (prev)
            *word_start = prev - buffer;
    }

    /* Move forward to find word end, stopping at whitespace */
    while (buffer[*word_end] && !isspace(buffer[*word_end]))
    {
        const char *next = utf8_next(buffer + *word_end);
        if (!next)
            break;
        *word_end = next - buffer;
    }
}

char *extract_command(const char *buffer, size_t cursor_pos)
{
    size_t start = 0;
    while (start < cursor_pos && isspace(buffer[start]))
    {
        start++;
    }
    size_t end = start;
    while (end < cursor_pos && !isspace(buffer[end]))
    {
        end++;
    }
    char *command = strndup(buffer + start, end - start);
    return command;
}

char *normalize_path(const char *path)
{
    if (!path || !*path)
    {
        return strdup(".");
    }

    /* Special case: if path is '..', return '../' to preserve relative parent directory */
    if (strcmp(path, "..") == 0)
    {
        return strdup("../");
    }

    char *result = malloc(MAX_PATH_LEN);
    if (!result)
    {
        return NULL;
    }
    result[0] = '\0';

    char *current = strdup(path);
    if (!current)
    {
        free(result);
        return NULL;
    }

    char *token = strtok(current, "/");
    char *stack[MAX_PATH_LEN];
    size_t stack_size = 0;

    if (path[0] == '/')
        strcpy(result, "/");

    while (token)
    {
        if (strcmp(token, ".") == 0)
        {
            /* Skip ./ */
        }
        else if (strcmp(token, "..") == 0)
        {
            /* Pop last directory if possible */
            if (stack_size > 0)
            {
                stack_size--;
            }
            else if (path[0] != '/')
            {
                /* For relative paths, push '..' if stack is empty */
                if (stack_size < MAX_PATH_LEN)
                {
                    stack[stack_size++] = token;
                }
            }
        }
        else if (*token)
        {
            /* Push valid directory/file */
            if (stack_size < MAX_PATH_LEN)
            {
                stack[stack_size++] = token;
            }
        }
        token = strtok(NULL, "/");
    }

    /* Build normalized path */
    size_t pos = strlen(result);
    for (size_t i = 0; i < stack_size; i++)
    {
        if (pos > 0 && result[pos - 1] != '/')
            result[pos++] = '/';
        strcpy(result + pos, stack[i]);
        pos += strlen(stack[i]);
    }

    if (pos == 0)
        strcpy(result, path[0] == '/' ? "/" : ".");

    free(current);
    return result;
}

int is_valid_var_name(const char *str)
{
    if (!str[0])
        return 0;
    if (!isalpha(str[0]) && str[0] != '_')
        return 0;
    for (size_t i = 0; str[i]; i++)
    {
        if (!isalnum(str[i]) && str[i] != '_')
            return 0;
    }
    return 1;
}

char *my_strsep(char **stringp, const char *delim) {
    if (stringp == NULL || *stringp == NULL) {
        return NULL;
    }
    char *start = *stringp;
    char *p = start;
    while (*p) {
        const char *d = delim;
        while (*d) {
            if (*p == *d) {
                *p = '\0';
                *stringp = p + 1;
                return start;
            }
            d++;
        }
        p++;
    }
    *stringp = NULL;
    return start;
}

char *get_home_dir_for_user(const char *username) {
    FILE *fp = tiny_fopen("/etc/passwd", "r");
    if (!fp) {
        return NULL;
    }

    char line[1024];
    while (tiny_fgets(line, sizeof(line), fp)) {
        char *line_ptr = line;
        char *user = my_strsep(&line_ptr, ":");
        if (user && tiny_strcmp(user, username) == 0) {
            my_strsep(&line_ptr, ":"); // password
            my_strsep(&line_ptr, ":"); // uid
            my_strsep(&line_ptr, ":"); // gid
            my_strsep(&line_ptr, ":"); // gecos
            char *home = my_strsep(&line_ptr, ":");
            tiny_fclose(fp);
            // remove trailing newline
            if (home) {
                char *end = home + strlen(home) - 1;
                if (*end == '\n') {
                    *end = '\0';
                }
            }
            return tiny_strdup(home);
        }
    }

    tiny_fclose(fp);
    return NULL;
}
