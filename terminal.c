#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <termios.h>
#include <errno.h>
#include <ctype.h>
#include <sys/ioctl.h>

#define MAX_INPUT_LENGTH  8192
#define MAX_SUGGESTIONS   256

// Update the shell prompt
void update_prompt(char *prompt, size_t size) {
    char cwd[1024];
    char temp[1024];
    const char *home = getenv("HOME");
    const char *user = getenv("USER") ? : "user";
    const char *hostname = getenv("HOSTNAME") ? : "localhost";

    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd");
        snprintf(cwd, sizeof(cwd), "[unknown]");
    } else if (home && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(temp, sizeof(temp), "~%s", cwd + strlen(home));
        strncpy(cwd, temp, sizeof(cwd));  // Copy temp back to cwd safely
    }

    int prompt_len;
    if (geteuid() == 0)
        prompt_len = snprintf(prompt, size, "[%s@%s %s]# ", user, hostname, cwd);
    else
        prompt_len = snprintf(prompt, size, "[%s@%s %s]$ ", user, hostname, cwd);

    if (prompt_len >= (int)size) {
        fprintf(stderr, "Warning: Prompt string truncated.\n");
        prompt[size - 1] = '\0';
    }
}

// Move cursor by a specified offset
static void move_cursor(int offset) {
    if (offset > 0) {
        printf("\033[%dC", offset);  // Move cursor right
    } else if (offset < 0) {
        printf("\033[%dD", -offset);  // Move cursor left
    }
    fflush(stdout);
}

// Handle backspace key
static void backspace(size_t *pos, char *buffer, size_t *buffer_size) {
    if (*pos > 0) {
        // Move cursor left and erase the character
        printf("\b \b");

        // Update buffer and shift characters
        (*pos)--;
        (*buffer_size)--;
        memmove(buffer + (*pos), buffer + (*pos) + 1, *buffer_size - (*pos) + 1);

        // Redraw the line after the backspace operation
        printf("%s", buffer + *pos);

        // Clear the extra character left at the end
        size_t end_len = *buffer_size - (*pos);
        if (end_len > 0) {
            printf("%*s", (int)end_len + 1, " "); // Clear extra characters
            printf("\b"); // Move cursor back by one position
        }

        // Move cursor back to the original position
        move_cursor(-(int)end_len);

        fflush(stdout);
    }
}

// List possible command completions
static int list_commands(const char *prefix, char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH]) {
    char *path = getenv("PATH");
    char *dir = strtok(path, ":");
    int suggestion_count = 0;

    while (dir && suggestion_count < MAX_SUGGESTIONS) {
        DIR *dp = opendir(dir);
        if (dp) {
            struct dirent *entry;
            while ((entry = readdir(dp)) != NULL) {
                if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0) {
                    strncpy(suggestions[suggestion_count++], entry->d_name, MAX_INPUT_LENGTH);
                }
            }
            closedir(dp);
        }
        dir = strtok(NULL, ":");
    }
    return suggestion_count;
}

// List possible path completions
static int list_files(const char *path, const char *prefix, char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH]) {
    DIR *dir = opendir(path);
    int suggestion_count = 0;

    if (!dir) return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && suggestion_count < MAX_SUGGESTIONS) {
        if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0) {
            strncpy(suggestions[suggestion_count++], entry->d_name, MAX_INPUT_LENGTH);
        }
    }

    closedir(dir);
    return suggestion_count;
}

// Get terminal width
static int get_terminal_width() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}

// Print suggestions in a grid format
static void print_suggestions_grid(char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH], int suggestion_count) {
    if (suggestion_count == 0) return;

    int max_len = 0;
    for (int i = 0; i < suggestion_count; i++) {
        int len = strlen(suggestions[i]);
        if (len > max_len) {
            max_len = len;
        }
    }
    max_len += 2;  // Add spacing between columns

    int term_width = get_terminal_width();
    int cols = term_width / max_len;

    for (int i = 0; i < suggestion_count; i++) {
        printf("%-*s", max_len, suggestions[i]);
        if ((i + 1) % cols == 0 || i == suggestion_count - 1) {
            printf("\n");
        }
    }
}

// Tab completion function
static void tab_complete(const char *prompt, char *buffer, size_t *pos, size_t *buffer_size) {
    if (*buffer_size == 0) {
        // Do nothing if the buffer is empty
        return;
    }

    char *first_space = strchr(buffer, ' ');
    int is_path = buffer[0] == '.' || buffer[0] == '/';
    char suggestions[MAX_SUGGESTIONS][MAX_INPUT_LENGTH];
    int suggestion_count = 0;

    if (first_space == NULL) {
        // We're completing the first word (command or path)
        if (is_path) {
            // Handle path completion
            char *last_slash = strrchr(buffer, '/');
            if (last_slash) {
                // Separate the path and the prefix
                *last_slash = '\0';
                const char *path = buffer;
                const char *prefix = last_slash + 1;
                suggestion_count = list_files(path, prefix, suggestions);
                *last_slash = '/';
            } else {
                suggestion_count = list_files(".", buffer, suggestions);
            }
        } else {
            // Handle command completion
            suggestion_count = list_commands(buffer, suggestions);
        }
    } else if (is_path) {
        // Completing a path in an argument
        char *last_slash = strrchr(first_space + 1, '/');
        if (last_slash) {
            *last_slash = '\0';
            const char *path = first_space + 1;
            const char *prefix = last_slash + 1;
            suggestion_count = list_files(path, prefix, suggestions);
            *last_slash = '/';
        } else {
            suggestion_count = list_files(".", first_space + 1, suggestions);
        }
    }

    if (suggestion_count == 1) {
        // If there's only one suggestion, auto-complete it
        size_t suggestion_len = strlen(suggestions[0]);
        strncpy(buffer + *pos, suggestions[0] + (first_space ? strlen(first_space + 1) : strlen(buffer)), suggestion_len);
        *pos += suggestion_len;
        *buffer_size = *pos;
        buffer[*buffer_size] = '\0';
        printf("%s", buffer + *pos - suggestion_len);
        fflush(stdout);
    } else if (suggestion_count > 1) {
        // If there are multiple suggestions, list them in grid format
        printf("\n");
        print_suggestions_grid(suggestions, suggestion_count);

        // Reprint the prompt, buffer, and move the cursor back to its position
        printf("%s%s", prompt, buffer);
        move_cursor(-(int)(*buffer_size - *pos));
        fflush(stdout);
    }
}

char *terminal_readline(const char *prompt) {
    char buffer[MAX_INPUT_LENGTH];
    struct termios oldt, newt;
    int ch;
    size_t pos = 0;
    size_t buffer_size = 0;

    // Set terminal to raw mode to handle input
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    printf("%s", prompt);
    fflush(stdout);

    while ((ch = getchar()) != EOF && ch != '\n') {
        if (ch == 27) { // Handle escape sequences for arrow keys
            getchar(); // Skip [
            switch (getchar()) {
                case 'A': // Up arrow
                    // Handle up arrow (history navigation, etc.)
                    break;
                case 'B': // Down arrow
                    // Handle down arrow (history navigation, etc.)
                    break;
                case 'C': // Right arrow
                    if (pos < buffer_size) {
                        move_cursor(1);
                        pos++;
                    }
                    break;
                case 'D': // Left arrow
                    if (pos > 0) {
                        move_cursor(-1);
                        pos--;
                    }
                    break;
            }
            continue;
        }
        if (ch == '\t') { // Handle tab completion
            tab_complete(prompt, buffer, &pos, &buffer_size);
            continue;
        }
        if (ch == 127) { // Handle backspace
            backspace(&pos, buffer, &buffer_size);
            continue;
        }
        if (!iscntrl(ch)) { // Handle regular characters
            if (buffer_size < MAX_INPUT_LENGTH - 1) {
                // Insert character by shifting everything to the right
                memmove(buffer + pos + 1, buffer + pos, buffer_size - pos);
                buffer[pos] = ch;
                buffer_size++;
                pos++;
                buffer[buffer_size] = '\0';

                // Redraw the line from the current position
                printf("\033[2K\r%s%s", prompt, buffer);
                move_cursor(-(int)(buffer_size - pos)); // Move cursor back to the end of the new input
                fflush(stdout);
            }
        }
    }

    // Reset terminal to original mode
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    buffer[buffer_size] = '\0';
    return strdup(buffer);
}

int read_multiline_input() {
    char prompt[1024];
    char *input;

    while (1) {
        update_prompt(prompt, sizeof(prompt));
        input = terminal_readline(prompt);

        if (input == NULL) {
            break;
        }

        if (strcmp(input, "exit") == 0) {
            free(input);
            break;
        }

        system(input);
        free(input);
    }

    return 0;
}
