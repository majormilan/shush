#include "tab.h"
#include "../builtins.h"
#include "../libtinyio/stdio.h"
#include "../libtinyio/string.h"
#include "config.h"
#include "readline.h"
#include "utf8.h"
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_COMPLETIONS 256
#define MAX_PATH_LEN 4096

static readline_completion_cb completion_callback = NULL;

/* Structure to hold completion state */
typedef struct
{
    char **candidates;
    size_t count;
    size_t index;
    char *original_word;
    char *command; /* Command being completed */
    int tab_count; /* Number of consecutive TAB presses */
} CompletionState;

static CompletionState completion_state = {0};

void tab_set_completion_callback(readline_completion_cb callback)
{
    completion_callback = callback;
}

/* Free completion state */
static void free_completion_state(CompletionState *state)
{
    if (state->candidates)
    {
        for (size_t i = 0; i < state->count; i++)
        {
            free(state->candidates[i]);
        }
        free(state->candidates);
        state->candidates = NULL;
    }
    if (state->original_word)
    {
        free(state->original_word);
        state->original_word = NULL;
    }
    if (state->command)
    {
        free(state->command);
        state->command = NULL;
    }
    state->count = 0;
    state->index = 0;
    state->tab_count = 0;
}

/* Determine if the cursor is in the command position (first word) */
static int is_command_position(const char *buffer, size_t cursor_pos)
{
    size_t i = 0;
    while (i < cursor_pos && isspace(buffer[i]))
    {
        i++;
    }
    if (i >= cursor_pos)
        return 1; /* Cursor is in leading whitespace */
    while (i < cursor_pos && !isspace(buffer[i]))
    {
        i++;
    }
    while (i < cursor_pos && isspace(buffer[i]))
    {
        i++;
    }
    return i >= cursor_pos; /* True if cursor is in first word or before second word */
}

/* Find the word boundaries under the cursor, preserving path prefixes and environment variables */
static void find_word_boundaries(const char *buffer, size_t cursor_pos,
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

/* Extract the command from the buffer */
static char *extract_command(const char *buffer, size_t cursor_pos)
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

/* Compare function for sorting candidates */
static int compare_strings(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

/* Find the longest common prefix of candidates */
static char *find_common_prefix(char **candidates, size_t count,
                                const char *word)
{
    if (count == 0)
    {
        return NULL;
    }
    if (count == 1)
    {
        return strdup(candidates[0]);
    }

    size_t min_len = strlen(candidates[0]);
    for (size_t i = 1; i < count; i++)
    {
        size_t len = strlen(candidates[i]);
        if (len < min_len)
            min_len = len;
    }

    char *prefix = malloc(min_len + 1);
    if (!prefix)
    {
        return NULL;
    }

    size_t i;
    for (i = 0; i < min_len; i++)
    {
        char c = candidates[0][i];
        for (size_t j = 1; j < count; j++)
        {
            if (candidates[j][i] != c)
            {
                prefix[i] = '\0';
                return prefix;
            }
        }
        prefix[i] = c;
    }
    prefix[i] = '\0';
    return prefix;
}

/* Check if a string is a valid environment variable name */
static int is_valid_var_name(const char *str)
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

/* Normalize a path by resolving ./ and ../ */
static char *normalize_path(const char *path)
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

    char *saveptr;
    char *token = strtok_r(current, "/", &saveptr);
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
        token = strtok_r(NULL, "/", &saveptr);
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

/* Default completion: filenames, $PATH executables, or environment variables based on context */
static char **get_default_completions(const char *command, const char *word,
                                      size_t *count, int is_command,
                                      char **path_prefix)
{
    char **candidates = malloc(MAX_COMPLETIONS * sizeof(char *));
    if (!candidates)
    {
        return NULL;
    }

    *count = 0;
    size_t word_len = strlen(word);

    /* Environment variable completion */
    if (!is_command && word[0] == '$' && word_len > 1)
    {
        const char *var_prefix = word + 1; /* Skip the '$' */
        size_t var_prefix_len = word_len - 1;

        /* Only attempt env var completion if var_prefix is valid */
        if (is_valid_var_name(var_prefix))
        {
            extern char **environ;
            for (int i = 0; environ[i] && *count < MAX_COMPLETIONS; i++)
            {
                char *var = environ[i];
                char *eq = strchr(var, '=');
                if (eq)
                {
                    size_t var_len = eq - var;
                    char *var_name = strndup(var, var_len);
                    if (var_name)
                    {
                        if (is_valid_var_name(var_name) &&
                            var_len >= var_prefix_len &&
                            strncmp(var_name, var_prefix, var_prefix_len) == 0)
                        {
                            candidates[*count] = malloc(var_len + 2);
                            if (candidates[*count])
                            {
                                strcpy(candidates[*count], "$");
                                strcat(candidates[*count], var_name);
                                (*count)++;
                            }
                        }
                        free(var_name);
                    }
                }
            }

            if (*count > 0)
            {
                /* Sort candidates alphabetically */
                if (*count > 1)
                {
                    qsort(candidates, *count, sizeof(char *), compare_strings);
                }

                *path_prefix = strdup("");
                if (!*path_prefix)
                {
                    for (size_t i = 0; i < *count; i++)
                        free(candidates[i]);
                    free(candidates);
                    return NULL;
                }
                return candidates;
            }
        }
    }

    /* Handle special case: word is just "/" or ends with "/" */
    int is_directory_completion = (word_len > 0 && word[word_len - 1] == '/');
    char *path = strdup(word);
    if (!path)
    {
        free(candidates);
        return NULL;
    }
    char *basename = NULL;
    char *dir_path = NULL;
    size_t basename_len = word_len;

    if (is_directory_completion)
    {
        dir_path = word_len == 1 ? "/" : path;
        basename = "";
        basename_len = 0;
        *path_prefix = strdup(dir_path);
        if (!*path_prefix)
        {
            free(path);
            free(candidates);
            return NULL;
        }
    }
    else
    {
        char *last_slash = strrchr(path, '/');
        if (last_slash)
        {
            *last_slash = '\0';
            basename = last_slash + 1;
            dir_path = path[0] == '/' ? path : (*path ? path : ".");
            char *normalized_dir = normalize_path(dir_path);
            if (!normalized_dir)
            {
                free(path);
                free(candidates);
                return NULL;
            }
            size_t dir_path_len = strlen(normalized_dir);
            *path_prefix = malloc(dir_path_len + 2);
            if (!*path_prefix)
            {
                free(normalized_dir);
                free(path);
                free(candidates);
                return NULL;
            }
            strcpy(*path_prefix, normalized_dir);
            if (dir_path_len > 0 && normalized_dir[dir_path_len - 1] != '/')
            {
                strcat(*path_prefix, "/");
            }
            free(normalized_dir);
            basename_len = strlen(basename);
        }
        else
        {
            basename = path;
            char cwd[MAX_PATH_LEN];
            if (getcwd(cwd, sizeof(cwd)) == NULL)
            {
                dir_path = ".";
            }
            else
            {
                dir_path = cwd;
            }
            basename_len = word_len;
            *path_prefix = strdup("");
            if (!*path_prefix)
            {
                free(path);
                free(candidates);
                return NULL;
            }
        }
    }

    if (is_command)
    {
        /* Command completion: handle relative paths or $PATH executables */
        if (strchr(word, '/'))
        {
            /* Relative path executable completion */
            DIR *dir = opendir(dir_path);
            if (dir)
            {
                struct dirent *entry;
                while ((entry = readdir(dir)) && *count < MAX_COMPLETIONS)
                {
                    /* Skip hidden files unless basename starts with '.' */
                    if (entry->d_name[0] == '.' && basename[0] != '.')
                        continue;
                    if (strncmp(entry->d_name, basename, basename_len) == 0)
                    {
                        char full_path[MAX_PATH_LEN];
                        snprintf(full_path, sizeof(full_path), "%s/%s",
                                 dir_path, entry->d_name);
                        if (access(full_path, X_OK) == 0)
                        {
                            candidates[*count] = strdup(entry->d_name);
                            if (candidates[*count])
                            {
                                (*count)++;
                            }
                        }
                    }
                }
                closedir(dir);
            }
        }
        else
        {
            /* $PATH executable completion */
            char *seen = calloc(MAX_COMPLETIONS, MAX_PATH_LEN);
            if (!seen)
            {
                free(path);
                free(*path_prefix);
                free(candidates);
                return NULL;
            }
            size_t seen_count = 0;

            char *path_env = getenv("PATH");
            if (path_env)
            {
                char *path_copy = strdup(path_env);
                if (path_copy)
                {
                    char *dir = strtok(path_copy, ":");
                    while (dir && *count < MAX_COMPLETIONS)
                    {
                        DIR *dirp = opendir(dir);
                        if (dirp)
                        {
                            struct dirent *entry;
                            while ((entry = readdir(dirp)) &&
                                   *count < MAX_COMPLETIONS)
                            {
                                /* Skip hidden files unless word starts with '.' */
                                if (entry->d_name[0] == '.' && word[0] != '.')
                                    continue;
                                if (strncmp(entry->d_name, word, word_len) == 0)
                                {
                                    char full_path[MAX_PATH_LEN];
                                    snprintf(full_path, sizeof(full_path),
                                             "%s/%s", dir, entry->d_name);
                                    if (access(full_path, X_OK) == 0)
                                    {
                                        /* Check for duplicates */
                                        int is_duplicate = 0;
                                        for (size_t i = 0; i < seen_count; i++)
                                        {
                                            if (strcmp(seen + i * MAX_PATH_LEN,
                                                       entry->d_name) == 0)
                                            {
                                                is_duplicate = 1;
                                                break;
                                            }
                                        }
                                        if (!is_duplicate)
                                        {
                                            candidates[*count] =
                                                strdup(entry->d_name);
                                            if (candidates[*count])
                                            {
                                                strncpy(seen + seen_count *
                                                                   MAX_PATH_LEN,
                                                        entry->d_name,
                                                        MAX_PATH_LEN - 1);
                                                seen_count++;
                                                (*count)++;
                                            }
                                        }
                                    }
                                }
                            }
                            closedir(dirp);
                        }
                        dir = strtok(NULL, ":");
                    }
                    free(path_copy);
                }
            }
            free(seen);
        }
    }
    else
    {
        /* Try built-in completion first */
        candidates = builtin_completion(command, word, count);
        if (!candidates)
        {
            /* Argument completion: filenames in specified directory */
            candidates = malloc(MAX_COMPLETIONS * sizeof(char *));
            if (!candidates)
            {
                free(path);
                free(*path_prefix);
                return NULL;
            }
            *count = 0;
            DIR *dir = opendir(dir_path);
            if (dir)
            {
                struct dirent *entry;
                while ((entry = readdir(dir)) && *count < MAX_COMPLETIONS)
                {
                    /* Skip hidden files unless basename starts with '.' */
                    if (entry->d_name[0] == '.' && basename[0] != '.')
                        continue;
                    if (strncmp(entry->d_name, basename, basename_len) == 0)
                    {
                        candidates[*count] = strdup(entry->d_name);
                        if (candidates[*count])
                        {
                            (*count)++;
                        }
                    }
                }
                closedir(dir);
            }
        }
    }

    free(path);

    /* Sort candidates alphabetically */
    if (*count > 1)
    {
        qsort(candidates, *count, sizeof(char *), compare_strings);
    }

    return candidates;
}

/* Apply a completion to the buffer, preserving path prefix */
static void apply_completion(char *buffer, size_t *len, size_t *cursor_pos,
                             size_t word_start, size_t word_end,
                             const char *completion, const char *word,
                             int is_command, const char *path_prefix,
                             int is_partial)
{
    /* Check if completion is a directory (for argument completion) */
    int is_directory = 0;
    int add_space = 0;
    char full_completion[MAX_PATH_LEN];

    if (!is_command && !is_partial)
    {
        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s%s", path_prefix, completion);
        struct stat st;
        if (stat(full_path, &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                is_directory = 1;
            }
            else
            {
                add_space = 1;
            }
        }
    }
    else if (is_command)
    {
        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s%s", path_prefix, completion);
        if (access(full_path, X_OK) == 0)
        {
            add_space = 1; /* Add space for executables */
        }
    }

    /* Construct full completion: prefix + completion + trailing chars */
    snprintf(full_completion, sizeof(full_completion), "%s%s%s%s", path_prefix,
             completion, is_directory ? "/" : "",
             add_space && !is_partial ? " " : "");

    size_t completion_len = strlen(full_completion);
    size_t word_len = word_end - word_start;
    size_t new_len = *len - word_len + completion_len;

    if (new_len >= BUFFER_SIZE - 1)
    {
        return; /* Buffer overflow check */
    }

    if (word_end < *len)
    {
        memmove(buffer + word_start + completion_len, buffer + word_end,
                *len - word_end);
    }
    memcpy(buffer + word_start, full_completion, completion_len);
    *len = new_len;
    buffer[*len] = '\0';
    *cursor_pos = word_start + completion_len;
}

/* List completion candidates */
static void list_completions(CompletionState *state, int prompt_row,
                             int prompt_col, struct termios *orig_termios)
{
    if (state->count == 0)
    {
        return;
    }

    if (state->count > 20)
    {
        char prompt[256];
        snprintf(prompt, sizeof(prompt),
                 "\nThere are more than %zu possibilities, do you want to list "
                 "them? [y/n] ",
                 state->count);
        printf("%s", prompt);
        fflush(stdout);

        disable_raw_mode(orig_termios);
        char response = getchar();
        enable_raw_mode(orig_termios);

        printf("\n");
        if (response != 'y' && response != 'Y')
        {
            return;
        }
        fflush(stdout);
    }

    printf("\n");
    for (size_t i = 0; i < state->count; i++)
    {
        printf("%s  ", state->candidates[i]);
        if ((i + 1) % 5 == 0)
            printf("\n"); /* Simple column formatting */
    }
    printf("\n");
    fflush(stdout);
}

/* Handle tab completion */
void tab_complete(const char *prompt, char *buffer, size_t *len,
                  size_t *cursor_pos, size_t prompt_width, int prompt_row,
                  int prompt_col, struct termios *orig_termios)
{
    size_t word_start, word_end;
    find_word_boundaries(buffer, *cursor_pos, &word_start, &word_end);

    char *word = strndup(buffer + word_start, word_end - word_start);
    if (!word)
    {
        return;
    }

    /* Do nothing if word is empty */
    if (strlen(word) == 0)
    {
        free(word);
        return;
    }

    int is_command = is_command_position(buffer, *cursor_pos);
    char *command =
        is_command ? strdup(word) : extract_command(buffer, *cursor_pos);
    if (!command)
    {
        free(word);
        return;
    }

    /* Reset state if word or command has changed */
    if (completion_state.original_word && completion_state.command &&
        (strcmp(word, completion_state.original_word) != 0 ||
         strcmp(command, completion_state.command) != 0))
    {
        free_completion_state(&completion_state);
    }

    char *path_prefix = NULL;
    if (!completion_state.candidates)
    {
        /* Generate new completions */
        if (completion_callback)
        {
            completion_state.candidates =
                completion_callback(command, word, &completion_state.count);
        }
        else
        {
            completion_state.candidates =
                get_default_completions(command, word, &completion_state.count,
                                        is_command, &path_prefix);
        }
        completion_state.index = 0;
        completion_state.original_word = strdup(word);
        completion_state.command = strdup(command);
        completion_state.tab_count = 1;
    }
    else
    {
        completion_state.tab_count++;
        /* Recompute path_prefix for listing */
        char *temp_path = strdup(word);
        if (temp_path)
        {
            char *last_slash = strrchr(temp_path, '/');
            if (last_slash)
            {
                *last_slash = '\0';
                char *normalized_dir = normalize_path(temp_path);
                if (normalized_dir)
                {
                    size_t len = strlen(normalized_dir);
                    path_prefix = malloc(len + 2);
                    if (path_prefix)
                    {
                        strcpy(path_prefix, normalized_dir);
                        if (len > 0 && normalized_dir[len - 1] != '/')
                        {
                            strcat(path_prefix, "/");
                        }
                    }
                    free(normalized_dir);
                }
            }
            else
            {
                path_prefix = strdup("");
            }
            free(temp_path);
        }
    }

    if (completion_state.candidates)
    {
        if (completion_state.count == 1)
        {
            /* Single candidate: apply immediately */
            apply_completion(buffer, len, cursor_pos, word_start, word_end,
                             completion_state.candidates[0], word, is_command,
                             path_prefix, 0);
            free_completion_state(&completion_state);
        }
        else if (completion_state.count > 1)
        {
            if (completion_state.tab_count == 1)
            {
                /* First TAB: apply common prefix */
                char *prefix = find_common_prefix(completion_state.candidates,
                                                  completion_state.count, word);
                if (prefix && strlen(prefix) > 0)
                {
                    /* Check if prefix matches any candidate exactly or is a directory */
                    int exact_match = 0;
                    int is_directory = 0;
                    char full_path[MAX_PATH_LEN];
                    snprintf(full_path, sizeof(full_path), "%s%s", path_prefix,
                             prefix);
                    struct stat st;
                    if (!is_command && stat(full_path, &st) == 0 &&
                        S_ISDIR(st.st_mode))
                    {
                        is_directory = 1;
                    }
                    for (size_t i = 0; i < completion_state.count; i++)
                    {
                        if (strcmp(prefix, completion_state.candidates[i]) == 0)
                        {
                            exact_match = 1;
                            break;
                        }
                    }

                    /* Use prefix as completion string, no extra dot */
                    apply_completion(buffer, len, cursor_pos, word_start,
                                     word_end, prefix, word, is_command,
                                     path_prefix,
                                     !exact_match && !is_directory);
                }
                free(prefix);
            }
            else
            {
                /* Second and subsequent TABs: list completions */
                list_completions(&completion_state, prompt_row, prompt_col,
                                 orig_termios);
            }
        }
        redraw_line(prompt, buffer, *cursor_pos, prompt_width, prompt_row,
                    prompt_col);
    }

    free(word);
    free(command);
    free(path_prefix);
}
