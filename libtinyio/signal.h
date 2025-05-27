#include <sys/types.h>
#ifndef TINY_SIGNAL_H
#define TINY_SIGNAL_H

/* Define signal numbers */
#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGABRT 6
#define SIGFPE 8
#define SIGKILL 9
#define SIGSEGV 11
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGUSR1 10
#define SIGUSR2 12
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGIO 29
#define SIGPWR 30
#define SIGSYS 31

/* Define signal function types */
typedef void (*sighandler_t)(int);

/* Function declarations */
char *tiny_strsignal(int sig);
int sig_from_name(const char *name);
sighandler_t signal(int signum, sighandler_t handler);
int kill(pid_t pid, int sig);

#define strsignal tiny_strsignal
#endif /* TINY_SIGNAL_H */
