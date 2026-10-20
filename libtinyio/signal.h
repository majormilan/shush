#include <sys/types.h>
#include <signal.h> /* real sighandler_t, SIGxxx, kill(), signal(), sigaction() */
#ifndef TINY_SIGNAL_H
#define TINY_SIGNAL_H

/* Function declarations */
char *tiny_strsignal(int sig);
int sig_from_name(const char *name);

#define strsignal tiny_strsignal
#endif /* TINY_SIGNAL_H */
