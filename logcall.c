/*
 *  logcall.c - agwpe linux replacement
 *
 *  Copyright (C) 2010, Lieven De Samblanx, ON7LDS. All rights reserved
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


#include "logcall.h"

#include <netax25/daemon.h>


// For older kernels
#ifndef PF_PACKET
#define PF_PACKET PF_INET
#endif

#define DWELL 10000


int signal_action = 0;


/*--------------------------------------------------------------------------*/

static void signal_action_proc(int sig)
{
    signal_action = sig;
}

/*--------------------------------------------------------------------------*/

static void terminate(int sig)
{

	exit(0);
}

/*--------------------------------------------------------------------------*/
void lprintf(int dtype, char *fmt, ...)
{
        va_list args;
        char str[1024];
        unsigned char *p;
        int sevenbit = 1;

        va_start(args, fmt);
        vsnprintf(str, 1024, fmt, args);
        va_end(args);

        for (p = str; *p != '\0'; p++)
                if ((*p < 32 && *p != '\n')
                    || (*p > 126 && *p < 160 && sevenbit))
                        *p = '.';
        strcat(lprintbuf,str);
}


/*--------------------------------------------------------------------------*/
static void check_monitor(int monsock){

    fd_set monavail;
    struct timeval tv;
    struct sockaddr monfrom;
    socklen_t monfromlen;
    int size;
    char buf[BUFSIZE];
    struct ifreq ifr;


	// ------ Check if there is new data on the monitor socket
	FD_ZERO(&monavail);
	FD_SET(monsock, &monavail);
	tv.tv_sec = 0;
	tv.tv_usec = DWELL;
	select(monsock + 1, &monavail, 0, 0, &tv);
	if (FD_ISSET(monsock, &monavail)) {
		monfromlen = sizeof(monfrom);
		size = recvfrom(monsock, buf, sizeof(buf), 0, &monfrom, &monfromlen);
		//------ Check if we have received a AX.25-packet
		strcpy(ifr.ifr_name, monfrom.sa_data);
		ioctl(monsock, SIOCGIFHWADDR, &ifr);
		    if (ifr.ifr_hwaddr.sa_family == AF_AX25) {
#ifdef NEW_AX25_STACK
			ax25_dump(buf, size, 0);
#else
			ki_dump(buf, size, 0);
#endif

		}  // end if AX25
	} // end if
}


/*--------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	int monsock;
	int become_daemon,n;

	sigset_t blockset;

	become_daemon = 0;

	while ((n = getopt(argc, argv, "dlv")) != -1) {
		switch (n) {
			case 'd':
				become_daemon = TRUE;
				break;
			case 'v':
				printf("\nldsped logcall version %s\n", LOGCALL_VERSION);
				printf ("Copyright (C) 2010 ON7LDS. All rights reserved.\n\n");
				return 0;
			case '?':
			case ':':
				fprintf(stderr, "usage: ldsped  [-d] [-v]\n");
				return 1;
		}
	}


	writelog(0, "ldsped logcall v%s starting ...",LOGCALL_VERSION); 


	// ------------------------------ start program -----------------------------------------

	if (become_daemon) {
	    if (!daemon_start(FALSE)) {
		writelog(0, "ldsped: cannot become a daemon");
		return 1;
	    } else {
		fclose(stdout);
		fclose(stderr);
		fclose(stdin);
	    }
	} else printf("Started in console mode.\n");


// ------------------------------------------- debug -------------------------------------------
//	file1 = fopen("/tmp/F1", "a");
//	file2 = fopen("/tmp/F2", "a");
// ------------------------------------------- debug -------------------------------------------


	// ------------------------------ initialising ------------------------------------------
	init_ax_config_calls();

	// Set some signal handlers
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, terminate);
	signal(SIGUSR1, signal_action_proc);
	signal(SIGUSR2, signal_action_proc);

	sigemptyset(&blockset);
	sigaddset(&blockset,SIGUSR1);
	sigaddset(&blockset,SIGUSR2);

	sigprocmask(SIG_BLOCK, &blockset, NULL);

	// Open socket for monitoring traffic
	if ((monsock = socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ALL))) < 0) {
//		writelog(0, "Error opening monitor socket: m/%s", strerror(errno));
		exit(1);
	}

	fcntl(monsock, F_SETFL, O_NONBLOCK);

//	writelog(6,"========> Open fd %i in main",monsock);

	// ------------------------------ start main loop ---------------------------------------
	while (1) {

		// check if there is data on the AX25 socket
		check_monitor(monsock);

		// only process the signals here. We do not want to interrupt
		// the functions above or the accompaying systemcalls
		sigprocmask(SIG_UNBLOCK, &blockset, NULL);
		if (signal_action == SIGUSR1) { dump_info(); signal_action = 0; }
		if (signal_action == SIGUSR2) { ; signal_action = 0; }
		sigprocmask(SIG_BLOCK, &blockset, NULL);

	} // end main while loop

	close(monsock);
	closelog();
	return 0;
}
