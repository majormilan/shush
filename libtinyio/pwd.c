#include "pwd.h"
#include "stdio.h"
#include <stdlib.h>
#include "string.h"

#define LINE_BUFFER_SIZE 256

/* Helper function to parse a line from /etc/passwd */
static struct passwd *parse_passwd_line(char *line) {
    static struct passwd pw;
    static char buffer[LINE_BUFFER_SIZE];
    
    strcpy(buffer, line);
    
    pw.pw_name = strtok(buffer, ":");
    pw.pw_passwd = strtok(NULL, ":");
    pw.pw_uid = (uid_t)atoi(strtok(NULL, ":"));
    pw.pw_gid = (gid_t)atoi(strtok(NULL, ":"));
    pw.pw_gecos = strtok(NULL, ":");
    pw.pw_dir = strtok(NULL, ":");
    pw.pw_shell = strtok(NULL, ":");
    
    return &pw;
}

/* Retrieves the user information based on the user ID (uid) */
struct passwd *tiny_getpwuid(uid_t uid) {
    FILE *passwd_file = fopen("/etc/passwd", "r");
    if (!passwd_file) {
        return NULL;
    }
    
    char line[LINE_BUFFER_SIZE];
    while (fgets(line, sizeof(line), passwd_file)) {
        struct passwd *pw = parse_passwd_line(line);
        if (pw->pw_uid == uid) {
            fclose(passwd_file);
            return pw;
        }
    }
    
    fclose(passwd_file);
    return NULL; /* User not found */
}

/* Retrieves the user information based on the username */
struct passwd *tiny_getpwnam(const char *name) {
    FILE *passwd_file = fopen("/etc/passwd", "r");
    if (!passwd_file) {
        return NULL;
    }
    
    char line[LINE_BUFFER_SIZE];
    while (fgets(line, sizeof(line), passwd_file)) {
        struct passwd *pw = parse_passwd_line(line);
        if (strcmp(pw->pw_name, name) == 0) {
            fclose(passwd_file);
            return pw;
        }
    }
    
    fclose(passwd_file);
    return NULL; /* User not found */
}
