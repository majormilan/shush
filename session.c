/*  session.c */
#include "session.h"
#include "libtinyio/pwd.h"
#include "libtinyio/stdio.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void initialize_session(Session *session)
{
    if (!session)
        return;

    session->uid = getuid();
    session->gid = getgid();

    struct passwd *pw = getpwuid(session->uid);
    if (pw)
    {
        session->username = strdup(pw->pw_name);
        session->home_dir = strdup(pw->pw_dir);
    }
    else
    {
        session->username = NULL;
        session->home_dir = NULL;
    }
}

void update_session(Session *session)
{
    if (!session)
        return;

    session->uid = getuid();
    session->gid = getgid();

    struct passwd *pw = getpwuid(session->uid);
    if (pw)
    {
        if (session->username)
            free(session->username);
        if (session->home_dir)
            free(session->home_dir);

        session->username = strdup(pw->pw_name);
        session->home_dir = strdup(pw->pw_dir);
    }
}

void print_session_info(const Session *session)
{
    if (!session)
        return;

    printf("Session Info:\n");
    printf("UID: %d\n", session->uid);
    printf("GID: %d\n", session->gid);
    if (session->username)
    {
        printf("Username: %s\n", session->username);
    }
    if (session->home_dir)
    {
        printf("Home Directory: %s\n", session->home_dir);
    }
}
