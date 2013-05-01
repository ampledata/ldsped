/*
    Adapted from beacon.c for ldsped
    by Lieven De Samblanx, ON7LDS.
*/


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>


#ifdef HAVE_NETAX25_AX25_H
#include <netax25/ax25.h>
#else
#include <netax25/kernel_ax25.h>
#endif
#ifdef HAVE_NETROSE_ROSE_H
#include <netrose/rose.h>
#else 
#include <netax25/kernel_rose.h>
#endif

#include <netax25/axlib.h>
#include <netax25/axconfig.h>
#include <netax25/daemon.h>

#include <ldsped.h>
#include <agw.h>
#include <utils.h>

int sendbeacon(AGWFrame frame, char *message, int length, char *specialdest)
{
    struct full_sockaddr_ax25 dest;
    struct full_sockaddr_ax25 src;
    int s,  dlen, len, i, portL,portH;
    char addr[20];
    char *destination;

    message[length]=0;
    writelog(4, "UI data to send (%c-frame).  Port %d, PID %i, From <%s>, To <%s> Data length %i",
		frame.DataKind, frame.Port , frame.PID, frame.CallFrom, frame.CallTo, frame.DataLen);
    if ( (message[0]==':') && (message[10]==':') )
	writelog(4, "Message <%s>",message);
    else
	writelog(4, "Message <%s> Destination <%s>",message,specialdest);

	if ( strcmp(frame.CallTo, "APAGW") == 0 )
	    strcpy(frame.CallTo, APDEST);

	if (specialdest==NULL)
	    destination=frame.CallTo;
	else
	    destination=specialdest;

	if ((dlen = ax25_aton(destination, &dest)) == -1) {
	    writelog(0, "ERROR: unable to convert destination callsign '%s'", destination);
	    return 1;
	}
    // port 255 = send the message on each port, respectively
	if (frame.Port == 255)
	    { portL=0; portH=ports-1; }
	    else 
	    { portL=frame.Port; portH=portL;};


	for (i=portL; i<=portH; i++) {
	    if (strcmp(frame.CallFrom, portlist[i].call) != 0)
		    sprintf(addr, "%s %s", frame.CallFrom, portlist[i].call);
	    else
		    strcpy(addr, portlist[i].call);

	    if ((len = ax25_aton(addr, &src)) == -1) {
		    writelog(0, "ERROR: sb/unable to convert source callsign '%s'", frame.CallFrom);
		    return 1;
	    }

	    if ((s = socket(AF_AX25, SOCK_DGRAM, 0)) == -1) {
		    writelog(0, "ERROR sb/socket: %m");
		    return 1;
	    }

		writelog(6,"========> Open fd %i in sendbeacon",s);

	    if (bind(s, (struct sockaddr *)&src, len) == -1) {
		    writelog(0, "ERROR sb/bind: %m");
		    return 1;
	    }
	    writelog(4,"Sending UI data on port %i - <%s> <%s>",i, message,destination);
	    if (sendto(s, message, length, 0, (struct sockaddr *)&dest, dlen) == -1) {
		    writelog(0, "ERROR sb/sendto: %m");
		    return 1;
	    } else {
			if ( (message[0]==':') && (message[10]==':') )
			    {
				destination=message; destination[11]=0;
				//note that the message string now is altered !
			    }
			writelog(4,"UI data sent (destination: %s)",destination); }

	    if (close(s)<0)
		writelog(0, "ERROR: sb/closing socket, %s",strerror(errno));
	}
    return 0;
};
