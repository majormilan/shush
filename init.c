
#include "init.h"
#include "libtinyio/stdio.h"
#include "libtinyio/string.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_HOSTNAME_LENGTH 1024

char *home_directory = NULL;

/* Global variables to hold environment strings */
char hostname_env[MAX_HOSTNAME_LENGTH + 10] = "HOSTNAME=";
char path_env[] = "PATH=/bin:/usr/bin";

/* Set the hostname environment variable */
static void set_hostname(void)
{
    int fd = open("/etc/hostname", O_RDONLY);
    if (fd < 0)
    {
        perror("Error opening /etc/hostname");
        strcat(hostname_env, "hostname");
    }
    else
    {
        char hostname[MAX_HOSTNAME_LENGTH];
        ssize_t bytes_read = read(fd, hostname, MAX_HOSTNAME_LENGTH - 1);
        close(fd);

        if (bytes_read > 0)
        {
            hostname[bytes_read] = '\0'; /* Null-terminate the string */
            size_t len = strlen(hostname);
            if (len > 0 && hostname[len - 1] == '\n')
            {
                hostname[len - 1] = '\0'; /* Remove trailing newline */
            }
            strcat(hostname_env, hostname);
        }
        else
        {
            perror("Error reading hostname");
            strcat(hostname_env, "hostname");
        }
    }

    if (putenv(hostname_env) != 0)
    {
        perror("Error setting HOSTNAME");
        _exit(EXIT_FAILURE);
    }
}

/* Initialize the shell environment */
void initialize_shell(void)
{
    home_directory = getenv("HOME");
    if (!home_directory)
    {
        perror("HOME not set");
        _exit(EXIT_FAILURE);
    }

    set_hostname();

    if (getenv("PATH") == NULL)
    {
        if (putenv(path_env) != 0)
        {
            perror("Error setting PATH");
            _exit(EXIT_FAILURE);
        }
    }
}
