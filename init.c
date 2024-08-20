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

char *home_directory = NULL;
char *hostname = NULL;

void
initialize_shell(void)
{
    home_directory = getenv("HOME");
    if (!home_directory) {
        fprintf(stderr, "HOME not set\n");
        exit(EXIT_FAILURE);
    }

    hostname = malloc(1024);  // Allocate memory for hostname
    if (!hostname) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    if (gethostname(hostname, 1024) < 0) {
        perror("gethostname");
        strcpy(hostname, "hostname");
    }

    if (setenv("PATH", "/bin:/usr/bin", 1) < 0) {
        perror("setenv");
        exit(EXIT_FAILURE);
    }
}
