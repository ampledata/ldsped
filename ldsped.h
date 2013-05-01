/*
 *  ldsped.h
 *
 *  Copyright (C) 2006, Lieven De Samblanx, ON7LDS. All rights reserved
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef LDSPED_HEADERS
#define LDSPED_HEADERS 1


#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#ifdef HAVE_NETAX25_AX25_H
#include <netax25/ax25.h>
#else
#include <netax25/kernel_ax25.h>
#endif
#include <netax25/axconfig.h>

#define LDSPED_VERSION "1.16"


#define READWRITE 1
#define KFRAMES   2
#define MFRAMES   4
#define MRTGSTATS 8

#define DATA_MHEARD_FILE	AX25_LOCALSTATEDIR"mheard/mheard.dat"

#define CONFFILE 		AX25_SYSCONFDIR"ldsped.conf"
#define MAX_SOCKETS             5
#define MAX_CONNECTS           10
#define MAX_PORTS              20
#define MAX_LOGINS             20
#define MAX_REGISTRATION_CALLS 50
#define MAX_REGISTRATION_CONNS 100
#define MAX_PROC_CONNECTIONS   100
#define MAX_SCRIPTS            30
#define MAX_SCRIPT_CHILDS      5
#define MAX_CHILD_EXEX_TIME    5


#define BUFSIZE          1500


typedef struct ExtConnData
{
    int    fd;
    int    nr;
    unsigned int addrlen;
    struct sockaddr_in addr;
    int   mode;

} ExtConnData;

struct ExtConnData connection[MAX_CONNECTS];


typedef struct sockets
{
    int mode;
    int list;
    char filename[100];
} sockets;

struct sockets socks[MAX_SOCKETS];


typedef struct registration_conn_data
{
    int ax;
    int tcp;
    char portnr;
    char from[15];
    char to[15];
    long connect;

} registration_conn_data;

struct registration_conn_data registration_conn[MAX_REGISTRATION_CONNS];


typedef struct registration_call_type
{
    char call[12];
    int tcp_fd;
} registration_call_type;

struct registration_call_type registration_call[MAX_REGISTRATION_CALLS];


typedef struct procinfo_connection_type
{
    char src[12];
    char dst[12];
    int  sendQ;
    int  state;
} procinfo_connection_type;

struct procinfo_connection_type procinfo_connections[MAX_PROC_CONNECTIONS];


typedef struct portlisttype
{
    char dev[20];
    char name[20];
    char call[20];
    int  baud;
    int  bytecount;
    int  bytecount2m;
} portlisttype;

portlisttype portlist[MAX_PORTS];

typedef struct scripttype
{
    char name[10];
    char path[127];
} scripttype;

scripttype scripts[MAX_SCRIPTS];


typedef struct script_child_type
{
    int PID;
    char name[10];
    time_t starttime;
} script_child_type;

script_child_type scriptchild[MAX_SCRIPT_CHILDS];

char login[MAX_LOGINS][25];
char APDEST[10];
int conn_num;
int check_proc;
int logins, registrations, registration_conns;
int logging, become_daemon, Hframes, PORTINFO_STRICT;
int sock_num;
int ax_txdelay, ax_txtail, ax_persist, ax_slottime, ax_maxframe;
long byte_count_timer_val;
int scriptuser,scriptgroup,scriptpaths,scriptchilds;

// ------------------------------------------- debug -------------------------------------------
FILE *file1,*file2;
// ------------------------------------------- debug -------------------------------------------

char lprintbuf[BUFSIZE];

void close_connection(int index);

#endif
