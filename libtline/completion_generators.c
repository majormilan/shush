#include "completion_generators.h"
#include "completion_utils.h"
#include "../builtins.h"
#include "../libtinyio/stdio.h"
#include "../libtinyio/string.h"
#include "config.h"
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>



char **complete_environment_variables(const char *word, size_t *count, char **path_prefix) {
    size_t word_len = strlen(word);
    if (word[0] != '$' || word_len <= 1) {
        return NULL;
    }

    const char *var_prefix = word + 1; /* Skip the '$' */
    size_t var_prefix_len = word_len - 1;
    if (!is_valid_var_name(var_prefix)) {
        return NULL;
    }

    char **candidates = malloc(MAX_COMPLETIONS * sizeof(char *));
    if (!candidates) {
        return NULL;
    }

    *count = 0;
    extern char **environ;
    for (int i = 0; environ[i] && *count < MAX_COMPLETIONS; i++) {
        char *var = environ[i];
        char *eq = strchr(var, '=');
        if (eq) {
            size_t var_len = eq - var;
            char *var_name = strndup(var, var_len);
            if (var_name) {
                if (is_valid_var_name(var_name) &&
                    var_len >= var_prefix_len &&
                    strncmp(var_name, var_prefix, var_prefix_len) == 0) {
                    candidates[*count] = malloc(var_len + 2);
                    if (candidates[*count]) {
                        strcpy(candidates[*count], "$");
                        strcat(candidates[*count], var_name);
                        (*count)++;
                    }
                }
                free(var_name);
            }
        }
    }

    if (*count > 0) {
        if (*count > 1) {
            qsort(candidates, *count, sizeof(char *), compare_strings);
        }
        *path_prefix = strdup("");
        return candidates;
    }

    free(candidates);
    return NULL;
}

char **complete_paths(const char *command, const char *word, size_t *count, char **path_prefix, int is_command) {
    size_t word_len = strlen(word);
    char *path = strdup(word);
    if (!path) {
        return NULL;
    }

    if (word[0] == '~') {
        char *home_dir = NULL;
        char *remaining_path = NULL;
        char *expanded_path = NULL;
        int user_lookup = 0;

        char *slash = strchr(word, '/');
        if (slash) {
            *slash = '\0';
            remaining_path = slash + 1;
        }

        if (strlen(word) == 1) { // Just "~"
            home_dir = getenv("HOME");
        } else { // "~user"
            home_dir = get_home_dir_for_user(word + 1);
            user_lookup = 1;
        }

        if (home_dir) {
            if (remaining_path) {
                expanded_path = malloc(strlen(home_dir) + 1 + strlen(remaining_path) + 1);
                if (expanded_path) {
                    strcpy(expanded_path, home_dir);
                    strcat(expanded_path, "/");
                    strcat(expanded_path, remaining_path);
                }
            } else {
                expanded_path = strdup(home_dir);
            }
            if (user_lookup) {
                free(home_dir);
            }
        }

        if (slash) {
            *slash = '/';
        }
        
        if (expanded_path) {
            free(path);
            path = expanded_path;
            word_len = strlen(path);
        }
    }

    char **candidates = malloc(MAX_COMPLETIONS * sizeof(char *));
    if (!candidates) {
        free(path);
        return NULL;
    }
    *count = 0;

    int is_directory_completion = (word_len > 0 && path[word_len - 1] == '/');
    char *basename = NULL;
    char *dir_path = NULL;
    size_t basename_len = word_len;

    if (is_directory_completion) {
        dir_path = word_len == 1 ? "/" : path;
        basename = "";
        basename_len = 0;
        *path_prefix = strdup(dir_path);
    } else {
        char *last_slash = strrchr(path, '/');
        if (last_slash) {
            basename = last_slash + 1;
            if (last_slash == path) {
                dir_path = "/";
            } else {
                *last_slash = '\0';
                dir_path = path;
            }
            char *normalized_dir = normalize_path(dir_path);
            if (normalized_dir) {
                size_t dir_path_len = strlen(normalized_dir);
                *path_prefix = malloc(dir_path_len + 2);
                if (*path_prefix) {
                    strcpy(*path_prefix, normalized_dir);
                    if (dir_path_len > 0 && normalized_dir[dir_path_len - 1] != '/') {
                        strcat(*path_prefix, "/");
                    }
                }
                free(normalized_dir);
            }
            basename_len = strlen(basename);
        } else {
            basename = path;
            char cwd[MAX_PATH_LEN];
            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                dir_path = ".";
            } else {
                dir_path = cwd;
            }
            basename_len = word_len;
            *path_prefix = strdup("");
        }
    }

    DIR *dir = opendir(dir_path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) && *count < MAX_COMPLETIONS) {
            if (entry->d_name[0] == '.' && basename[0] != '.') {
                continue;
            }
            if (strncmp(entry->d_name, basename, basename_len) == 0) {
                char full_path[MAX_PATH_LEN];
                snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
                struct stat st;
                if (stat(full_path, &st) != 0) {
                    continue;
                }

                if (strcmp(command, "cd") == 0) {
                    if (S_ISDIR(st.st_mode)) {
                        candidates[*count] = strdup(entry->d_name);
                        if (candidates[*count]) {
                            (*count)++;
                        }
                    }
                } else {
                    if (is_command) {
                        if (access(full_path, X_OK) == 0) {
                            candidates[*count] = strdup(entry->d_name);
                            if (candidates[*count]) {
                                (*count)++;
                            }
                        }
                    } else {
                        candidates[*count] = strdup(entry->d_name);
                        if (candidates[*count]) {
                            (*count)++;
                        }
                    }
                }
            }
        }
        closedir(dir);
    }

    free(path);

    if (*count > 0) {
        if (*count > 1) {
            qsort(candidates, *count, sizeof(char *), compare_strings);
        }
        return candidates;
    }

    free(candidates);
    return NULL;
}

char **complete_commands(const char *word, size_t *count, char **path_prefix) {
    char **candidates = malloc(MAX_COMPLETIONS * sizeof(char *));
    if (!candidates) {
        return NULL;
    }
    *count = 0;

    char *seen = calloc(MAX_COMPLETIONS, MAX_PATH_LEN);
    if (!seen) {
        free(candidates);
        return NULL;
    }
    size_t seen_count = 0;

    char *path_env = getenv("PATH");
    if (path_env) {
        char *path_copy = strdup(path_env);
        if (path_copy) {
            char *dir = strtok(path_copy, ":");
            while (dir && *count < MAX_COMPLETIONS) {
                DIR *dirp = opendir(dir);
                if (dirp) {
                    struct dirent *entry;
                    while ((entry = readdir(dirp)) && *count < MAX_COMPLETIONS) {
                        if (entry->d_name[0] == '.' && word[0] != '.') {
                            continue;
                        }
                        if (strncmp(entry->d_name, word, strlen(word)) == 0) {
                            char full_path[MAX_PATH_LEN];
                            snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
                            if (access(full_path, X_OK) == 0) {
                                int is_duplicate = 0;
                                for (size_t i = 0; i < seen_count; i++) {
                                    if (strcmp(seen + i * MAX_PATH_LEN, entry->d_name) == 0) {
                                        is_duplicate = 1;
                                        break;
                                    }
                                }
                                if (!is_duplicate) {
                                    candidates[*count] = strdup(entry->d_name);
                                    if (candidates[*count]) {
                                        strncpy(seen + seen_count * MAX_PATH_LEN, entry->d_name, MAX_PATH_LEN - 1);
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

    if (*count > 0) {
        if (*count > 1) {
            qsort(candidates, *count, sizeof(char *), compare_strings);
        }
        *path_prefix = strdup("");
        return candidates;
    }

    free(candidates);
    return NULL;
}
