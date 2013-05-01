#ifndef AX25CONN_HEADERS
#define AX25CONN_HEADERS 1


#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>


#ifdef HAVE_NETAX25_AX25_H
#include <netax25/ax25.h>
#else
#include <netax25/kernel_ax25.h>
#endif

#include <netax25/axlib.h>
#include <netax25/axconfig.h>

#include <utils.h>

int  ax25connect(char *, char *, char **);
void ax25disconnect(int,int);
int  write_ax25data(int fd, const void *buf, size_t count);

void add_ax25connection(int ,int, int, char *, char *);
int  find_ax25connection(char *, char *);
void close_ax25connections(int);

#endif
