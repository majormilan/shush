#include "completion_callbacks.h"
#include "completion_generators.h"
#include "completion_utils.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static char **cd_completion_callback(const char *command, const char *word, const char *buffer, size_t *count, char **path_prefix) {
    return complete_paths(command, word, count, path_prefix, 0);
}

static char **git_completion_callback(const char *command, const char *word, const char *buffer, size_t *count, char **path_prefix) {
    static const char *git_commands[] = {
        "add", "am", "archive", "bisect", "branch", "bundle", "checkout", "cherry-pick",
        "citool", "clean", "clone", "commit", "describe", "diff", "fetch", "format-patch",
        "gc", "gitk", "grep", "gui", "init", "log", "maintenance", "merge", "mv", "notes",
        "pull", "push", "range-diff", "rebase", "reset", "restore", "revert", "rm",
        "scalar", "shortlog", "show", "sparse-checkout", "stash", "status", "submodule",
        "switch", "tag", "worktree", NULL
    };

    // Git subcommands that typically take a path as an argument
    static const char *git_path_commands[] = {
        "add", "rm", "mv", "checkout", "restore", NULL
    };

    // Parse the buffer to get the git subcommand
    char *buffer_copy = strdup(buffer);
    if (!buffer_copy) {
        return NULL;
    }

    char *token;
    char *rest = buffer_copy;
    char *git_subcommand = NULL;

    // Skip "git" command itself
    token = strtok(rest, " ");
    if (token) {
        git_subcommand = strtok(NULL, " ");
    }

    char **result_candidates = NULL;

    if (git_subcommand) {
        // Check if the subcommand expects a path
        int expects_path = 0;
        for (int i = 0; git_path_commands[i]; i++) {
            if (strcmp(git_subcommand, git_path_commands[i]) == 0) {
                expects_path = 1;
                break;
            }
        }

        if (expects_path) {
            // If it expects a path, use complete_paths
            result_candidates = complete_paths(command, word, count, path_prefix, 0);
        } else {
            // Otherwise, complete git subcommands
            char **candidates = malloc(MAX_COMPLETIONS * sizeof(char *));
            if (!candidates) {
                free(buffer_copy);
                return NULL;
            }
            *count = 0;

            for (int i = 0; git_commands[i] && *count < MAX_COMPLETIONS; i++) {
                if (strncmp(git_commands[i], word, strlen(word)) == 0) {
                    candidates[*count] = strdup(git_commands[i]);
                    if (candidates[*count]) {
                        (*count)++;
                    }
                }
            }

            if (*count > 0) {
                if (*count > 1) {
                    qsort(candidates, *count, sizeof(char *), compare_strings);
                }
                *path_prefix = strdup("");
                result_candidates = candidates;
            } else {
                free(candidates);
            }
        }
    } else { // No subcommand yet, complete git subcommands
        char **candidates = malloc(MAX_COMPLETIONS * sizeof(char *));
        if (!candidates) {
            free(buffer_copy);
            return NULL;
        }
        *count = 0;

        for (int i = 0; git_commands[i] && *count < MAX_COMPLETIONS; i++) {
            if (strncmp(git_commands[i], word, strlen(word)) == 0) {
                candidates[*count] = strdup(git_commands[i]);
                if (candidates[*count]) {
                    (*count)++;
                }
            }
        }

        if (*count > 0) {
            if (*count > 1) {
                qsort(candidates, *count, sizeof(char *), compare_strings);
            }
            *path_prefix = strdup("");
            result_candidates = candidates;
        } else {
            free(candidates);
        }
    }

    free(buffer_copy);
    return result_candidates;
}

static CompletionCallback completion_callbacks[] = {
    {"cd", cd_completion_callback},
    {"git", git_completion_callback},
    {NULL, NULL}
};

completion_callback_func get_completion_callback(const char *command) {
    for (int i = 0; completion_callbacks[i].command; i++) {
        if (strcmp(command, completion_callbacks[i].command) == 0) {
            return completion_callbacks[i].callback;
        }
    }
    return NULL;
}