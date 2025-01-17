/*  pwd.h */
#ifndef PWD_H
#define PWD_H

#include <sys/types.h>

struct passwd
{
    char *pw_name;   /*  Username */
    char *pw_passwd; /*  User password */
    uid_t pw_uid;    /*  User ID */
    gid_t pw_gid;    /*  Group ID */
    char *pw_gecos;  /*  Real name */
    char *pw_dir;    /*  Home directory */
    char *pw_shell;  /*  Shell program */
};

/*  Function prototypes */
struct passwd *tiny_getpwuid(uid_t uid);
struct passwd *tiny_getpwnam(const char *name);

#define getpwuid tiny_getpwuid
#define getpwnam tiny_getpwnam


#endif /*  PWD_H */
