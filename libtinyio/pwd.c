/*  pwd.c */
#include "pwd.h"
#include <stdlib.h>
#include <string.h>

/*  Simulated user database */
static struct passwd users[] = {
    {"root", "x", 0, 0, "root", "/root", "/bin/sh"},
    {"shush", "x", 1000, 1000, "shush", "/home/shush", "/usr/local/bin/shush"},
    {NULL, NULL, 0, 0, NULL, NULL, NULL} /*  End of list marker */
};

struct passwd *getpwuid(uid_t uid)
{
    for (int i = 0; users[i].pw_name != NULL; i++)
    {
        if (users[i].pw_uid == uid)
        {
            return &users[i];
        }
    }
    return NULL; /*  User not found */
}

struct passwd *getpwnam(const char *name)
{
    for (int i = 0; users[i].pw_name != NULL; i++)
    {
        if (strcmp(users[i].pw_name, name) == 0)
        {
            return &users[i];
        }
    }
    return NULL; /*  User not found */
}
