/*
 * MIT/X Consortium License
 * Copyright © 2024 Milán Atanáz Major
 *
 * Shell initialization for Simple Humane Shell (shush).
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "init.h"

#define MAX_HOSTNAME_LENGTH 1024

char *home_directory = NULL;

static void
set_hostname(void)
{
    FILE *file = fopen("/etc/hostname", "r");

    if (!file) {
        perror("fopen");
        if (setenv("HOSTNAME", "hostname", 1) < 0) {
            perror("setenv");
            exit(EXIT_FAILURE);
        }
        return;
    }

    char hostname[MAX_HOSTNAME_LENGTH];

    if (fgets(hostname, MAX_HOSTNAME_LENGTH, file) == NULL) {
        perror("fgets");
        if (setenv("HOSTNAME", "hostname", 1) < 0) {
            perror("setenv");
            exit(EXIT_FAILURE);
        }
    } else {
        size_t len = strlen(hostname);
        if (len > 0 && hostname[len - 1] == '\n') {
            hostname[len - 1] = '\0';
        }

        if (setenv("HOSTNAME", hostname, 1) < 0) {
            perror("setenv");
            exit(EXIT_FAILURE);
        }
    }

    fclose(file);
}

void
initialize_shell(void)
{
    home_directory = getenv("HOME");
    if (!home_directory) {
        fprintf(stderr, "HOME not set\n");
        exit(EXIT_FAILURE);
    }

    set_hostname();

    if (setenv("PATH", "/bin:/usr/bin", 1) < 0) {
        perror("setenv");
        exit(EXIT_FAILURE);
    }
}
