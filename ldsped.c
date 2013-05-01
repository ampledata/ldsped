/*
 *  ldsped.c - agwpe linux replacement
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


#include "ldsped.h"
#include "agw.h"
#include "utils.h"
#include "aprs.h"
#include "ax25connect.h"

#include <netax25/daemon.h>

// For older kernels
#ifndef PF_PACKET
#define PF_PACKET PF_INET
#endif

#define DWELL 10000

/*--------------------------------------------------------------------------*/

static union {
	struct sockaddr sa;
	struct sockaddr_in si;
	struct sockaddr_un su;
} addr;

/*--------------------------------------------------------------------------*/

int highest_sock_fd=0;
int signal_action = 0;

/*--------------------------------------------------------------------------*/

static struct sockaddr *build_sockaddr(const char *name, int *addrlen)
{
	char *host_name;
	char *serv_name;
	char buf[BUFSIZE];

	memset((char *) &addr, 0, sizeof(addr));
	*addrlen = 0;

	host_name = strcpy(buf, name);
	serv_name = strchr(buf, ':');
	if (!serv_name)
		return 0;
	*serv_name++ = 0;
	if (!*host_name || !*serv_name)
		return 0;

	if (!strcmp(host_name, "local") || !strcmp(host_name, "unix")) {
		addr.su.sun_family = AF_UNIX;
		*addr.su.sun_path = 0;
		strcat(addr.su.sun_path, serv_name);
		*addrlen = sizeof(struct sockaddr_un);
		return &addr.sa;
	}

	addr.si.sin_family = AF_INET;

	if (!strcmp(host_name, "*")) {
		addr.si.sin_addr.s_addr = INADDR_ANY;
	} else if (!strcmp(host_name, "loopback")) {
		addr.si.sin_addr.s_addr = inet_addr("127.0.0.1");
	} else if ((addr.si.sin_addr.s_addr = inet_addr(host_name)) == -1) {
		struct hostent *hp = gethostbyname(host_name);
		endhostent();
		if (!hp)
			return 0;
		addr.si.sin_addr.s_addr =
		    ((struct in_addr *) (hp->h_addr))->s_addr;
	}

	if (isdigit(*serv_name & 0xff)) {
		addr.si.sin_port = htons(atoi(serv_name));
	} else {
		struct servent *sp = getservbyname(serv_name, (char *) 0);
		endservent();
		if (!sp)
			return 0;
		addr.si.sin_port = sp->s_port;
	}

	*addrlen = sizeof(struct sockaddr_in);
	return &addr.sa;
}

/*--------------------------------------------------------------------------*/

static int add_socket(char *sockname, char monmode)
{
	struct sockaddr *saddr;
	int saddrlen;

	if (sock_num == MAX_SOCKETS - 1) {
		writelog(0,
			"WARNING: as/Too many sockets defined - only %d are "
			"allowed\n", MAX_SOCKETS);
		return -1;
	}

	if (!(saddr = build_sockaddr(sockname, &saddrlen))) {
		writelog(0, "WARNING: as/Invalid socket name: \"%s\"",
			sockname);
		return -1;
	}

	if (saddr->sa_family == AF_UNIX)
		strcpy(socks[sock_num].filename, strchr(sockname, ':') + 1);
	else
		socks[sock_num].filename[0] = 0;

	if ((socks[sock_num].list =
	     socket(saddr->sa_family, SOCK_STREAM, 0)) < 0) {
		writelog(0,
			"WARNING: as/Error opening socket \"%s\": %s",
			sockname, strerror(errno));
		return -1;
	}

	writelog(6,"========> Open fd %i in add_socket",socks[sock_num].list);


	if (bind(socks[sock_num].list, saddr, saddrlen) < 0) {
		writelog(0,
			"WARNING: as/Error binding socket \"%s\": %s",
			sockname, strerror(errno));
		return -1;
	}

	if (listen(socks[sock_num].list, 5) < 0) {
		writelog(0,
			"WARNING: as/Error listening on socket \"%s\": %s",
			sockname, strerror(errno));
		return -1;
	}

	fcntl(socks[sock_num].list, F_SETFL, O_NONBLOCK);

	if (socks[sock_num].list > highest_sock_fd)
		highest_sock_fd = socks[sock_num].list;
	writelog(1,"Socket %i : %s, rw=%i",sock_num,sockname,monmode);
	socks[sock_num].mode = monmode;
	sock_num++;

	return 0;
}

/*--------------------------------------------------------------------------*/


static void add_password(char *buf)
{
    if (logins<MAX_LOGINS) {
	if (strchr(buf, ' ')==NULL) {
	    writelog(0, "Passwordconfiguration not correct (%s).",buf);
	    return;
	} else {
	strncpy(login[logins],buf,24);
	login[logins][24]='\0';
	logins++;
	}
    } else {
	writelog(0, "Maximum registered passwords (%i) reached.",MAX_LOGINS);
    }
}

/*--------------------------------------------------------------------------*/

static void close_TCPIP_sockets()
{
	int i;

	writelog(0, "Closing all TCP/IP sockets ... ");
	for (i = 0; i < sock_num; i++) {
		if (close(socks[i].list)<0)
		    writelog(0, "ERROR: cts/closing socket %i/%i, %s",
				i,sock_num,strerror(errno));

		if (socks[i].filename[0])
			unlink(socks[i].filename);
	}
	writelog(0,"done.");	
}

/*--------------------------------------------------------------------------*/

static void close_AX25_sockets()
{
	int i;

	writelog(0, "Closing all AX25 sockets ... ");
	for (i = 0; i < registration_conns; i++) {
		if ( close(registration_conn[i].ax)<0)
		    writelog(0, "ERROR: cas/closing ax25 socket %i/%i, %s",
				i,registration_conns,strerror(errno));
	}
	writelog(0,"done.");	
}

/*--------------------------------------------------------------------------*/

void close_connection(int index) {
    if ( (index>=0) && (index<=conn_num) && (conn_num>0) ) {
	writelog(2, "Closing connection %i from %s:%i (%i left)",
				     connection[index].nr,
				     inet_ntoa(connection[index].addr.sin_addr),
				     ntohs(connection[index].addr.sin_port),
				     conn_num-1);
	UnRegisterCalls(connection[index].fd);
        if ( close(connection[index].fd)<0)
    	    writelog(0, "ERROR: cc/closing socket, %s",strerror(errno));
	if (socks[index].filename[0]) unlink(socks[index].filename);
        connection[index] = connection[conn_num - 1];
	conn_num--;
    }
}


/*--------------------------------------------------------------------------*/

static void signal_action_proc(int sig)
{
    signal_action = sig;
}

/*--------------------------------------------------------------------------*/

static void terminate(int sig)
{
    int i;

    for (i = 0; i < conn_num; i++)
		close(connection[i].fd);
	close_AX25_sockets();
	close_TCPIP_sockets();
//	close(monsock);
	if (logging) {
		writelog(0, "ldsped v%s terminated on SIGTERM",LDSPED_VERSION);
		closelog();
	}
	closelog();
	
	exit(0);
}

/*--------------------------------------------------------------------------*/
static void accept_connections(void) {
    struct timeval tv;
    fd_set conn_request;
    int i;

    // ------ Look for incoming connects on all open sockets,
    //        accept and add to the list
    FD_ZERO(&conn_request);
    for (i = 0; i < sock_num; i++)
    		FD_SET(socks[i].list, &conn_request);
    tv.tv_sec = 0;
    tv.tv_usec = DWELL;
    select(highest_sock_fd + 1, &conn_request, 0, 0, &tv);
    for (i = 0; i < sock_num; i++)
    	if (FD_ISSET(socks[i].list, &conn_request)) {
    		connection[conn_num].addrlen = sizeof(connection[conn_num].addr);
    		connection[conn_num].fd = accept(socks[i].list,
    						(struct sockaddr *) &connection[conn_num].addr,
    						&connection[conn_num].addrlen);
    		if ((logins) && (socks[i].mode != MRTGSTATS))
    		    connection[conn_num].mode = -1 * socks[i].mode ;
    		else
    		    connection[conn_num].mode = socks[i].mode;
    		connection[conn_num].nr=conn_num+1;
    		writelog(2, "Connection %i from %s:%i, socket %i (rw=%i)", 
    					connection[conn_num].nr,
    					inet_ntoa(connection[conn_num].addr.sin_addr),
    					ntohs(connection[conn_num].addr.sin_port),
    					i, connection[conn_num].mode);
    		conn_num++;
    		if (connection[conn_num-1].mode == MRTGSTATS) {
    		    put_traffic_stats(conn_num-1);
    		    close_connection(conn_num-1);
    		}
    	}
}
/*--------------------------------------------------------------------------*/

static void check_external(monsock) {

	int i,size;
	fd_set conn_l;
	struct timeval tv;
	struct sockaddr monfrom;
	socklen_t monfromlen;
	char buf[50];

		// ------ check if there is data on the external sockets
		for (i = 0; i < conn_num; i++) {


		    FD_ZERO(&conn_l);
		    FD_SET(connection[i].fd, &conn_l);
		    tv.tv_sec = 0;
		    tv.tv_usec = DWELL;
		    select(connection[i].fd + 1, &conn_l, 0, 0, &tv);
		    if (FD_ISSET(connection[i].fd, &conn_l)) {
			monfromlen = sizeof(monfrom);
			size = recvfrom(connection[i].fd, buf, 36, 0, &monfrom, &monfromlen);
			if (size > 0) { 
			    writelog(4, "External data on connection %i. Size = %i",connection[i].nr,size);
			    if (! processframe(buf,size,i,monsock)) {
				writelog(2,"Aborting connection");
				    close_connection(i);
			    }
			}
			if (size == 0) {
			    writelog(5, "   Connection %i lost.",connection[i].nr);
			    close_connection(i);
			}
		    }
		}
}

/*--------------------------------------------------------------------------*/

static void check_monitor(int monsock){

    fd_set monavail;
    struct timeval tv;
    struct sockaddr monfrom;
    socklen_t monfromlen;
    int i,size;
    char buf[BUFSIZE];
    struct ifreq ifr;
    char *message;


	// ------ Check if there is new data on the monitor socket
	FD_ZERO(&monavail);
	FD_SET(monsock, &monavail);
	tv.tv_sec = 0;
	tv.tv_usec = DWELL;
	select(monsock + 1, &monavail, 0, 0, &tv);
	port_byte_count(0,monfrom.sa_data);  // to get the 2 minutes timer right
	if (FD_ISSET(monsock, &monavail)) {
		monfromlen = sizeof(monfrom);
		size = recvfrom(monsock, buf, sizeof(buf), 0, &monfrom, &monfromlen);
		//------ Check if we have received a AX.25-packet
		strcpy(ifr.ifr_name, monfrom.sa_data);
		ioctl(monsock, SIOCGIFHWADDR, &ifr);
		    if (ifr.ifr_hwaddr.sa_family == AF_AX25) {
			i=count_packets(1);
			port_byte_count(size,monfrom.sa_data);

			clear_lprintbuf();
#ifdef NEW_AX25_STACK
			ax25_dump(buf, size, 0);
#else
			ki_dump(buf, size, 0);
#endif
			//------ check for Directed Queries (= message data type)
			message=strstr(lprintbuf,"\n");
			if (message != NULL)
			    {
				if (message[1]==':') check_APRS_message(buf,message);
			    }

			//------ Send the packet to all connected sockets asking for it :
			//~~~1)~~~ do monitor frames have to be send ?
			for (i = 0; i < conn_num; i++) {
			    if (connection[i].mode & MFRAMES) {

				if (write_lprintbuf(i, monfrom.sa_data)<0)
				    if (errno != EAGAIN) {
					writelog(0, "Error sending monitor data: cm1/%s", strerror(errno));
					close_connection(i);
				    }
			    }

			//~~~2)~~~ do raw data frames have to be send ?
			    if (connection[i].mode & KFRAMES)
				  if (send_frame('K', connection[i].fd, monfrom.sa_data, buf, size, "", "") < 0)
				    if (errno != EAGAIN) {
					writelog(0, "Error sending monitor data: cm2/%s", strerror(errno));
					close_connection(i);
				    }
			}
		}  // end if AX25
	} // end if
}


/*--------------------------------------------------------------------------*/
static void check_ax25connections() {
    int i,j,t,size;
    time_t seconds;
    fd_set conn_l,write_set;
    struct timeval tv;
    char buf[512],port[15];
    static char *ax25_state[5] =
        { "LISTENING", "SABM SENT", "DISC SENT", "ESTABLISHED", "RECOVERY" };

    seconds = time (NULL);

    for (i = 0; i < registration_conns; i++) {
	if (registration_conn[i].connect>0)
	{  // ------------ connection not yet established
	    {
		//  -------- check progress
	        FD_ZERO(&write_set);
	        FD_SET(registration_conn[i].ax,&write_set);
	        tv.tv_sec = 0;
	        tv.tv_usec = DWELL;
	        select (registration_conn[i].ax+1, 0, &write_set, 0, &tv);
	        if (FD_ISSET(registration_conn[i].ax, &write_set)) 
		  {
		    // 0"LISTENING", 1"SABM SENT", 2"DISC SENT", 3"ESTABLISHED", 4"RECOVERY"
		   t=search_connection_status(registration_conn[i].from, registration_conn[i].to);
		   if (t==3)
		    {
		    // -------- connected !
		    writelog(5,"   ---> AX25 connection %i is connected.",i+1);
		    writelog(2, "Connected : AX25 connection %i, from %s to %s (%i seconds attempt).",
		        i+1,registration_conn[i].from,registration_conn[i].to,seconds-registration_conn[i].connect);
		    sprintf(buf,"*** CONNECTED With Station %s\r",registration_conn[i].to);
		    registration_conn[i].connect=0;
		    j=registration_conn[i].portnr;
		    send_frame('C', registration_conn[i].tcp, portlist[j].name,
		            buf, strlen(buf)+1, registration_conn[i].to, registration_conn[i].from);
		    };
		    if (t==0)
		    {
		    writelog(5,"   ---> AX25 connection %i status %i (%s).",i+1,t,ax25_state[t]);
		    registration_conn[i].connect=1; //timeout
		    }
		  }
	    }
	    if (registration_conn[i].connect == 1)
	    {
	        // -------- TIMEOUT, connection not established
	        writelog(5,"   ---> AX25 connection %i timed out.",i);
	        writelog(2, "INFO: AX25 Connection %i from %s to %s timed out (%i left).",
	            i, registration_conn[i].from ,registration_conn[i].to, registration_conns-1);
	        sprintf(buf,"*** DISCONNECTED RETRYOUT With %s\r",registration_conn[i].to);
	        j=registration_conn[i].portnr;
	        send_frame('d', registration_conn[i].tcp, portlist[j].name,
	                buf, strlen(buf)+1, registration_conn[i].to, registration_conn[i].from);
	        ax25disconnect(i,0);
	        // end function now : after the disconnect above, the registration_conn[]
	        // table has changed. We do not want to iterate it further !
	        return;
	    } 
	} else {// ------------ existing connection
	   // -------- check if there is data on the AX25 sockets
	    FD_ZERO(&conn_l);
	    FD_SET(registration_conn[i].ax, &conn_l);
	    tv.tv_sec = 0;
	    tv.tv_usec = DWELL;
	    select(registration_conn[i].ax + 1, &conn_l, 0, 0, &tv);
	    if (FD_ISSET(registration_conn[i].ax, &conn_l)) {
		size = read(registration_conn[i].ax, buf, 511);
		if ( (size<0) && (errno != EWOULDBLOCK) && (errno != EAGAIN)) {
		    if (errno == ENOTCONN) {
			writelog(2, "INFO: AX25 Connection %i from %s to %s lost (%i left).",
			    i, registration_conn[i].from ,registration_conn[i].to, registration_conns-1);
			ax25disconnect(i,1); // connection lost
			// end function now : after the disconnect above, the registration_conn[]
			// table has changed. We do not want to iterate it further !
			return;
		    } else {
			writelog(0,"ERROR: cac/read: %s",strerror(errno));
			break;
		    }
		}
		if (size > 0) {
		    writelog(4, "Data on AX25 connection %i, from %s to %s. Size = %i",
				i+1,registration_conn[i].from, registration_conn[i].to, size);
		    j=registration_conn[i].portnr;
		    strcpy(port,portlist[j].name);
		    if (send_frame('D',
					registration_conn[i].tcp,
					port,
					buf, size,
					registration_conn[i].to,
					registration_conn[i].from) < 0)
			if (errno != EAGAIN) {
			    writelog(0, "ERROR cac/sending AX25 data: %s", strerror(errno));
			    //close_connection(i);
			}
		}
	    }
	}
    }  // end for
}


/*--------------------------------------------------------------------------*/


static void read_config(int complete) {

	FILE *conffile;
	char confline[100];
	char *mode;
	char *parameter;
	int n;
	
	PORTINFO_STRICT=0;
	
	if (complete == 0) {
	    writelog(0,"Re-reading config file. Socket definitions will be ignored.");
	    scriptpaths=0;
	}
	
	
	if (!(conffile = fopen(CONFFILE, "r"))) {
		fprintf(stderr, "Unable to open " CONFFILE ".\n");
		writelog(0, "Unable to open " CONFFILE ".");
		exit(1);
	}

	writelog(6,"========> Open fd ?? in read_config");


	while (fgets(confline, 100, conffile)) {
		if ( (confline[0] == '#') | (strlen(confline) < 7) )	// Comment or empty
			continue;

		confline[strlen(confline) - 1] = 0;	// Cut the LF

		if (!(parameter = strchr(confline, ' '))) {
			writelog(0,
				"WARNING: The following configuration line includes "
				"one or more errors:\n%s", confline);
			continue;
		}


		*(parameter++) = 0;
		mode = confline;
		n = 0;
		if (strcasecmp(mode, "LOGLEVEL") == 0)
			{if (atoi(parameter) < 6) logging=atoi(parameter);
			    else writelog(0,"ERROR: Logging parameter out of range"); }
		else if ( (strcasecmp(mode, "RO") == 0) && (complete) )
			n=add_socket(parameter, 0);
		else if ( (strcasecmp(mode, "RW") == 0)&& (complete) )
			n=add_socket(parameter, 1);
		else if (strcasecmp(mode, "PASSWORD") == 0)
			add_password(parameter);
		else if (strcasecmp(mode, "STATIONTEXT") == 0)
			init_station(parameter,1);
		else if (strcasecmp(mode, "TRAFFIC") == 0)
			init_traffic(parameter,1);
		else if (strcasecmp(mode, "TRAFFICSENDPORT") == 0)
			init_traffic(parameter,2);
		else if (strcasecmp(mode, "TRAFFICPOS") == 0)
			init_traffic(parameter,3);
		else if (strcasecmp(mode, "TRAFFICCALL") == 0)
			init_traffic(parameter,4);
		else if (strcasecmp(mode, "TRAFFICNAME") == 0)
			init_traffic(parameter,5);
		else if (strcasecmp(mode, "TRAFFICPATH") == 0)
			init_traffic(parameter,6);
		else if ( (strcasecmp(mode, "TRAFFICPORT") == 0) && (complete) )
			n=add_socket(parameter,8);
		else if (strcasecmp(mode, "PROBE") == 0)
			init_probe(parameter,1);
		else if (strcasecmp(mode, "PROBEPATH") == 0)
			init_probe(parameter,2);
		else if (strcasecmp(mode, "PROBESTRING") == 0)
			init_probe(parameter,3);
		else if (strcasecmp(mode, "PROBESENDPORT") == 0)
			init_probe(parameter,4);
		else if (strcasecmp(mode, "PROBECALL") == 0)
			init_probe(parameter,5);
		else if (strcasecmp(mode, "TX_DELAY") == 0)
			init_axprms(parameter,0);
		else if (strcasecmp(mode, "TX_TAIL") == 0)
			init_axprms(parameter,1);
		else if (strcasecmp(mode, "PERSIST") == 0)
			init_axprms(parameter,2);
		else if (strcasecmp(mode, "SLOTTIME") == 0)
			init_axprms(parameter,3);
		else if (strcasecmp(mode, "MAXFRAME") == 0)
			init_axprms(parameter,4);
		else if (strcasecmp(mode, "PORT_INFO_COMPATIBLE") == 0)
			{ PORTINFO_STRICT=atoi(parameter); 
			  if (PORTINFO_STRICT)
				writelog(0,"Using compatible portinfo string");
			    else writelog(0,"Using ldsped portinfo string");
			}
		else if (strcasecmp(mode, "SCRIPTUSER") == 0)
			init_script(parameter,0);
		else if (strcasecmp(mode, "SCRIPTGROUP") == 0)
			init_script(parameter,1);
		else if (strcasecmp(mode, "SCRIPTPATH") == 0)
			init_script(parameter,2);
		else
			writelog(0,
				"WARNING: Parameter \"%s\" unknown",
				mode);
		if (n<0) writelog(0, "ERROR : Socket %s could not be opened.",parameter);

	}
	if (check_parameters()<0) {
	    writelog(0, "FATAL: Configuration error.");
	    exit(1);
	    }
	if (logins>0) { writelog(1, "Password configuration : %i passwords.",logins); }
		else 	  {writelog(1, "No passwords configured. Free access."); }
		
	if (scriptpaths>0) { writelog(1, "Script configuration : %i scripts found.",scriptpaths); }
		else 	  {writelog(1, "No APRSQ scripts defined."); }
		
	if (fclose(conffile) < 0)
	    writelog(0, "ERROR: rc/closing file, %s",strerror(errno));

	if (complete)
	  if (sock_num == 0) {
		writelog(0, "FATAL: No usable socket found");
		exit(1);
	}

}


/*--------------------------------------------------------------------------*/
void betastart()
{

    time_t seconds, endseconds;
    long t;

    seconds = time (NULL);

    endseconds = 1254348000;
    t=endseconds;

    /*
	date -d '10/1/2009' +%s
	1254348000
    */

    writelog(0,"");
    if ( (seconds<0) || (seconds>t) )
	{
        writelog(0,"*******************************************************");
        writelog(0,"*  This is a beta version.                            *");
        writelog(0,"*   I know it's not good yet,                         *");
        writelog(0,"*   so this version did only run for a limited time.  *");
        writelog(0,"*******************************************************");
        writelog(0,"*  This version now is expired.                       *");
        writelog(0,"*   Contact ON7LDS,                                   *");
        writelog(0,"*   there should be a new version available.          *");
        writelog(0,"*******************************************************");
        exit(1);
	} else {
        writelog(0,"*******************************************************");
        writelog(0,"*  This is a beta version.                            *");
        writelog(0,"*   I know it's not good yet,                         *");
        writelog(0,"*   so this version will only run for a limited time. *");
        writelog(0,"*******************************************************");
        writelog(0,"*  Before this version expires,                       *");
        writelog(0,"*   there should be a new version available.          *");
        writelog(0,"*  Please mail me if you need a new version !         *");
        writelog(0,"*******************************************************");
	}
    writelog(0,"");

    if ( (seconds<0) || (seconds>t) ) exit(1);
}

/*--------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	int monsock;
	int i, n;

	sigset_t blockset;

	conn_num = 0;
	sock_num = 0;
	ports = -1;
	timestamp = 1;
	logging = 0;
	logins = 0;
	check_proc = 1;
	procinfo_connections[0].sendQ=-1;
	registrations = 0;
	registration_conns = 0;
	become_daemon = 0;
	byte_count_timer_val = 0;
	scriptuser = 65534;
	scriptgroup = 65534;
	scriptpaths = 0;
	scriptchilds = 0;

	openlog("ldsped", LOG_PID, LOG_DAEMON);
	clear_lprintbuf();
	clear_stats();

	while ((n = getopt(argc, argv, "dlv")) != -1) {
		switch (n) {
			case 'd':
				become_daemon = TRUE;
				break;
			case 'l':
				logging += 1;
//				if (logging>5) { printf("Please, do not exaggerate.\n"); return 1; }
// For debug purposes : 
				if (logging>6) { printf("Please, do not exaggerate.\n"); return 1; }
				break;
			case 'v':
				printf("\nldsped version %s\n", LDSPED_VERSION);
				printf ("Copyright (C) 2006-2010 ON7LDS. All rights reserved.\n\n");
				return 0;
			case '?':
			case ':':
				fprintf(stderr, "usage: ldsped  [-d] [-l[l][l][l][l]] [-v]\n");
				return 1;
		}
	}


	writelog(0, "ldsped v%s starting ...",LDSPED_VERSION); 

	//betastart();

	// ------------------------------ configuration file ------------------------------------

	init_probe(NULL,0);
	init_traffic(NULL,0);
	init_station(NULL,0);
	read_config(1);
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
	Hframes=checkmhearddata();

	strcpy(APDEST,"APLD  ");
	APDEST[4]=LDSPED_VERSION[0];
	APDEST[5]=LDSPED_VERSION[2];

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
		writelog(0, "Error opening monitor socket: m/%s", strerror(errno));
		exit(1);
	}

	fcntl(monsock, F_SETFL, O_NONBLOCK);

	writelog(6,"========> Open fd %i in main",monsock);

	// ------------------------------ start main loop ---------------------------------------
	while (1) {

		//Check the sendqueues of the OS
		check_proc = ax25_proc_info();

		// Accept TCP/IP connections from client programs
		accept_connections();

		// check if there is data on the TCP/IP sockets
		check_external(monsock);

		// check if there is data on the AX25 socket
		check_monitor(monsock);

		// check if there is data from AX25 connections
		check_ax25connections();

		// Check childs
		check_childs();

		// count the packets and send traffic beacon
		count_packets(0);

		// send probe
		check_probe();

		// only process the signals here. We do not want to interrupt
		// the functions above or the accompaying systemcalls
		sigprocmask(SIG_UNBLOCK, &blockset, NULL);
		if (signal_action == SIGUSR1) { dump_info(); signal_action = 0; }
		if (signal_action == SIGUSR2) { read_config(0); signal_action = 0; }
		sigprocmask(SIG_BLOCK, &blockset, NULL);

	} // end main while loop


// if I ever make a way out the loop, this will clean up

	for (i = 0; i < conn_num; i++)
	    close(connection[i].fd);
	close_AX25_sockets();
	close_TCPIP_sockets();
	close(monsock);
	closelog();
	return 0;
}
