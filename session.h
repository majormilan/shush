/*  session.h */
#ifndef SESSION_H
#define SESSION_H

#include <sys/types.h>

typedef struct
{
    uid_t uid;      /*  User ID */
    gid_t gid;      /*  Group ID */
    char *username; /*  Username */
    char *home_dir; /*  Home directory */
} Session;

extern Session session;

void initialize_session(Session *session);
void update_session(Session *session);
void print_session_info(const Session *session);

#endif /*  SESSION_H */
