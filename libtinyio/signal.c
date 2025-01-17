#include "signal.h"
#include <stddef.h> /*  for NULL */
#include <string.h> /*  for strcmp */

typedef struct
{
    int num;
    const char *name;
    const char *description;
} signal_entry_t;

static const signal_entry_t signal_table[] = {
    {SIGHUP, "HUP", "Hangup"},
    {SIGINT, "INT", "Interrupt"},
    {SIGQUIT, "QUIT", "Quit"},
    {SIGILL, "ILL", "Illegal instruction"},
    {SIGABRT, "ABRT", "Aborted"},
    {SIGFPE, "FPE", "Floating point exception"},
    {SIGKILL, "KILL", "Killed"},
    {SIGSEGV, "SEGV", "Segmentation fault"},
    {SIGPIPE, "PIPE", "Broken pipe"},
    {SIGALRM, "ALRM", "Alarm clock"},
    {SIGTERM, "TERM", "Terminated"},
    {SIGUSR1, "USR1", "User defined signal 1"},
    {SIGUSR2, "USR2", "User defined signal 2"},
    {SIGCHLD, "CHLD", "Child exited"},
    {SIGCONT, "CONT", "Continue"},
    {SIGSTOP, "STOP", "Stop"},
    {SIGTSTP, "TSTP", "Terminal stop"},
    {SIGTTIN, "TTIN", "Background read from tty"},
    {SIGTTOU, "TTOU", "Background write to tty"},
    {SIGURG, "URG", "Urgent condition on socket"},
    {SIGXCPU, "XCPU", "CPU time limit exceeded"},
    {SIGXFSZ, "XFSZ", "File size limit exceeded"},
    {SIGVTALRM, "VTALRM", "Virtual alarm clock"},
    {SIGPROF, "PROF", "Profiling timer expired"},
    {SIGWINCH, "WINCH", "Window size change"},
    {SIGIO, "IO", "I/O possible"},
    {SIGPWR, "PWR", "Power failure"},
    {SIGSYS, "SYS", "Bad system call"},
    {0, NULL, NULL} /*  Sentinel value */
};

const char *strsignal(int sig)
{
    for (const signal_entry_t *entry = signal_table; entry->name != NULL;
         ++entry)
    {
        if (entry->num == sig)
        {
            return entry->description;
        }
    }
    return "Unknown signal";
}

int sig_from_name(const char *name)
{
    for (const signal_entry_t *entry = signal_table; entry->name != NULL;
         ++entry)
    {
        if (strcmp(entry->name, name) == 0)
        {
            return entry->num;
        }
    }
    return -1; /*  Invalid signal name */
}

sighandler_t signal(int signum, sighandler_t handler)
{
    /* This is a stub implementation. Replace with actual signal handling if
     * needed */
    return (sighandler_t)0;
}
