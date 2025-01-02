/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Shell initialization for Simple Humane Shell (shush).
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h> // For open()

#include "init.h"

#define MAX_HOSTNAME_LENGTH 1024

char *home_directory = NULL;

static void
set_hostname(void)
{
    int fd = open("/etc/hostname", O_RDONLY);

    if (fd < 0) {
        write(STDERR_FILENO, "Error opening /etc/hostname\n", 28);
        if (setenv("HOSTNAME", "hostname", 1) < 0) {
            write(STDERR_FILENO, "Error setting HOSTNAME\n", 23);
            _exit(EXIT_FAILURE);
        }
        return;
    }

    char hostname[MAX_HOSTNAME_LENGTH];
    ssize_t bytes_read = read(fd, hostname, MAX_HOSTNAME_LENGTH - 1);

    if (bytes_read <= 0) {
        write(STDERR_FILENO, "Error reading hostname\n", 23);
        if (setenv("HOSTNAME", "hostname", 1) < 0) {
            write(STDERR_FILENO, "Error setting HOSTNAME\n", 23);
            _exit(EXIT_FAILURE);
        }
    } else {
        hostname[bytes_read] = '\0'; // Null-terminate the string
        size_t len = strlen(hostname);
        if (len > 0 && hostname[len - 1] == '\n') {
            hostname[len - 1] = '\0'; // Remove trailing newline
        }

        if (setenv("HOSTNAME", hostname, 1) < 0) {
            write(STDERR_FILENO, "Error setting HOSTNAME\n", 23);
            _exit(EXIT_FAILURE);
        }
    }

    close(fd);
}

void
initialize_shell(void)
{
    home_directory = getenv("HOME");
    if (!home_directory) {
        write(STDERR_FILENO, "HOME not set\n", 13);
        _exit(EXIT_FAILURE);
    }

    set_hostname();

    if (setenv("PATH", "/bin:/usr/bin", 1) < 0) {
        write(STDERR_FILENO, "Error setting PATH\n", 19);
        _exit(EXIT_FAILURE);
    }
}
