#include "completion.h"
#include "completion_utils.h"
#include "completion_generators.h"
#include "completion_callbacks.h"
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

void completion_set_callback(readline_completion_cb callback)
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

/* Default completion: filenames, $PATH executables, or environment variables based on context */
static char **get_default_completions(const char *command, const char *word, const char *buffer,
                                      size_t *count, int is_command,
                                      char **path_prefix)
{
    completion_callback_func callback = get_completion_callback(command);
    if (callback) {
        return callback(command, word, buffer, count, path_prefix);
    }

    if (!is_command && word[0] == '$') {
        return complete_environment_variables(word, count, path_prefix);
    }

    if (is_command && strchr(word, '/') == NULL) {
        return complete_commands(word, count, path_prefix);
    }

    return complete_paths(command, word, count, path_prefix, is_command);
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
    /* Ensure cursor is on a new line for prompt redisplay */
    printf("\r");
    fflush(stdout);
}

/* Handle tab completion */
void completion_complete(const char *prompt, char *buffer, size_t *len,
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
                completion_callback(command, word, buffer, &completion_state.count);
        }
        else
        {
            completion_state.candidates =
                get_default_completions(command, word, buffer, &completion_state.count,
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
            redraw_line(prompt, buffer, *cursor_pos, prompt_width, prompt_row,
                        prompt_col);
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
                redraw_line(prompt, buffer, *cursor_pos, prompt_width, prompt_row,
                            prompt_col);
            }
            else
            {
                /* Second and subsequent TABs: list completions */
                list_completions(&completion_state, prompt_row, prompt_col,
                                 orig_termios);
                /* Reprint prompt and buffer on a new line */
                printf("%s%s", prompt, buffer);
                fflush(stdout);
                /* Adjust cursor position to end of buffer */
                *cursor_pos = *len;
                /* Update prompt_row to reflect new line */
                prompt_row += (completion_state.count / 5) + 2; /* Approximate lines used */
                redraw_line(prompt, buffer, *cursor_pos, prompt_width, prompt_row,
                            prompt_col);
            }
        }
        else
        {
            /* No completions, just redraw the line */
            redraw_line(prompt, buffer, *cursor_pos, prompt_width, prompt_row,
                        prompt_col);
        }
    }
    else
    {
        /* No candidates, just redraw the line */
        redraw_line(prompt, buffer, *cursor_pos, prompt_width, prompt_row,
                    prompt_col);
    }

    free(word);
    free(command);
    free(path_prefix);
}