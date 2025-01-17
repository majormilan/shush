/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Shell initialization for Simple Humane Shell (shush).
 */

#include "libtinyio/string.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "init.h"

#define MAX_HOSTNAME_LENGTH 1024

char *home_directory = NULL;

// Global variables to hold environment strings
char hostname_env[MAX_HOSTNAME_LENGTH + 10] = "HOSTNAME=";
char path_env[] = "PATH=/bin:/usr/bin";

static void set_hostname(void)
{
    int fd = open("/etc/hostname", O_RDONLY);

    if (fd < 0)
    {
        write(STDERR_FILENO, "Error opening /etc/hostname\n", 28);
        strcat(hostname_env, "hostname");

        if (putenv(hostname_env) != 0)
        {
            write(STDERR_FILENO, "Error setting HOSTNAME\n", 23);
            _exit(EXIT_FAILURE);
        }
        return;
    }

    char hostname[MAX_HOSTNAME_LENGTH];
    ssize_t bytes_read = read(fd, hostname, MAX_HOSTNAME_LENGTH - 1);

    if (bytes_read <= 0)
    {
        write(STDERR_FILENO, "Error reading hostname\n", 23);
        strcat(hostname_env, "hostname");

        if (putenv(hostname_env) != 0)
        {
            write(STDERR_FILENO, "Error setting HOSTNAME\n", 23);
            _exit(EXIT_FAILURE);
        }
    }
    else
    {
        hostname[bytes_read] = '\0'; /*  Null-terminate the string */
        size_t len = strlen(hostname);
        if (len > 0 && hostname[len - 1] == '\n')
        {
            hostname[len - 1] = '\0'; /*  Remove trailing newline */
        }

        strcat(hostname_env, hostname);

        if (putenv(hostname_env) != 0)
        {
            write(STDERR_FILENO, "Error setting HOSTNAME\n", 23);
            _exit(EXIT_FAILURE);
        }
    }

    close(fd);
}

void initialize_shell(void)
{
    home_directory = getenv("HOME");
    if (!home_directory)
    {
        write(STDERR_FILENO, "HOME not set\n", 13);
        _exit(EXIT_FAILURE);
    }

    set_hostname();

    if (putenv(path_env) != 0)
    {
        write(STDERR_FILENO, "Error setting PATH\n", 19);
        _exit(EXIT_FAILURE);
    }
}
