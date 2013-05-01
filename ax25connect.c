#include "ldsped.h"
#include "ax25connect.h"


#define	MAX_PACKETLEN	512
#define	MAX_BUFLEN	2*MAX_PACKETLEN
#define min(a,b)	((a) < (b) ? (a) : (b))

/*nog aan OS vragen, verlopig vast zetten*/
int paclen_out = 255;


int interrupted = FALSE;


/*
void convert_cr_lf(char *buf, int len)
{
	while (len--) {
		if (*buf == '\r')
			*buf = '\n';
		buf++;
	}
}

void convert_lf_cr(char *buf, int len)
{
	while (len--) {
		if (*buf == '\n')
			*buf = '\r';
		buf++;
	}
}
*/

int write_ax25data(int fd, const void *buf, size_t count)
{
    int cnt = count;

    while (cnt > 0) {
	if (write(fd, buf, min(paclen_out, cnt)) < 0)
	    return -1;
	buf += paclen_out;
	cnt -= paclen_out;
    }
    return count;
}


int delete_ax25connection_nr(int conn_nr)
{
    int i;

    if (conn_nr>=0)
    {
	writelog(5,"   ---> Delete connectiondata %i (from %s to %s)",conn_nr,registration_conn[conn_nr].from,registration_conn[conn_nr].to);
	for (i=conn_nr;i<registration_conns;i++)
	    {
		registration_conn[i].ax =registration_conn[i+1].ax ;
		registration_conn[i].tcp=registration_conn[i+1].tcp;
		strcpy(registration_conn[i].from,registration_conn[i+1].from);
		strcpy(registration_conn[i].to  ,registration_conn[i+1].to  );
		registration_conn[i].connect=registration_conn[i+1].connect;
	    }
	registration_conns--;
//	writelog(2,"%i AX25 connections left",registration_conns);
	return 1;
    } else return 0;
}




int find_ax25connection(char *From, char *To)
{
    int i,j;

    j=-1;
    for (i=0;i<registration_conns;i++)
    {
	if ( (strcmp(registration_conn[i].from,From)==0) &&
	     (strcmp(registration_conn[i].to,To)==0) ) {
//		writelog(2, "Closing connection %i from %s to %s (%i left)",
//		    i+1,registration_conn[i].from, registration_conn[i].to, registration_conns-1);
//		close(registration_conn[i].ax);
		j=i;
	    }
    }
    return j;
}


void close_ax25connections(int fd)
{
    int i;

    i=0;

    writelog(5, "Closing all AX25 connections for tcp fd %i.",fd);
    while (i<registration_conns)
	{
	    writelog(5, "   --> disconnect connection (index %i) %s -> %s ?",
			i,registration_conn[i].from,registration_conn[i].to);
	    if (registration_conn[i].tcp==fd)
		{
		    ax25disconnect(i ,0);
		} else {
		    i++;
		}
    }
}



void ax25disconnect(int conn_nr, int sendmsg)
{
    AGWFrame answer;
    int i;
    char s1[100],s2[50];


    answer.res1=0;
    answer.res2=0;
    answer.res3=0;
    answer.res4=0;
    answer.res5=0;
    answer.res6=0;

    answer.Port=registration_conn[conn_nr].portnr;
    answer.DataKind='d';
    answer.PID=0xF0;
    strcpy(answer.CallFrom,registration_conn[conn_nr].from);
    strcpy(answer.CallTo,registration_conn[conn_nr].to);

    if (sendmsg) {
        //send disconnect message to application
        sprintf(s2,"*** DISCONNECTED From Station %s\r*",answer.CallTo);
        answer.DataLen=strlen(s2);
        bcopy(&answer,s1,36);
        for (i=0;i<strlen(s2);i++) { s1[36+i]=s2[i]; };
        s1[36+i-1]=0;
        write_buffer(connection[conn_nr].fd,s1,36+strlen(s2));
    }

    // close socket
    if ( close(registration_conn[conn_nr].ax)<0) 
	writelog(0, "ERROR: ad/closing socket, %s",strerror(errno));

    //delete connection data
    i=delete_ax25connection_nr(conn_nr);
    if (i<=0) writelog(0, "ERROR: could not delete connection data (fd=%i:%s->%s).",
			conn_nr,answer.CallFrom,answer.CallTo);

}


void add_ax25connection(int ax_fd, int tcp_fd, int portnr, char *from, char *to)
{
    if (ax_fd<0)
	{
	    writelog(0, "ERROR : Cannot add faulty connection fd");
	    return;
	}
    if (registration_conns>=MAX_REGISTRATION_CONNS)
	{
	    writelog(0, "ERROR : To many AX25 connections. Cannot add connection");
	    return;
	}
    registration_conn[registration_conns].ax=ax_fd;
    registration_conn[registration_conns].tcp=tcp_fd;
    registration_conn[registration_conns].portnr=portnr;
    strcpy(registration_conn[registration_conns].from,from);
    strcpy(registration_conn[registration_conns].to,to);
    registration_conn[registration_conns].connect=time(NULL);
    registration_conns++;
//    writelog(2, "Connected : AX25 connection %i, from %s to %s.",registration_conns,from,to);
}



int ax25connect(char *port, char *from, char *to[])
{
	int flags;
	char buffer[256], *addr;
	int err,fd, addrlen = sizeof(struct full_sockaddr_ax25);
	struct full_sockaddr_ax25 axbind, axconnect;

	axconnect.fsa_ax25.sax25_family = axbind.fsa_ax25.sax25_family = AF_AX25;
	axbind.fsa_ax25.sax25_ndigis = 1;

/* port */
	if ((addr = ax25_config_get_addr(port)) == NULL) {
		sprintf(buffer, "ERROR: axc/invalid AX.25 port name - %s\n", port);
		writelog(0,buffer);
	}

	if (ax25_aton_entry(addr, axbind.fsa_digipeater[0].ax25_call) == -1) {
		sprintf(buffer, "ERROR: axc/invalid AX.25 port callsign - %s\n", port);
		writelog(0,buffer);
	}

/*from call*/
	if (ax25_aton_entry(from, axbind.fsa_ax25.sax25_call.ax25_call) == -1) {
		sprintf(buffer, "ERROR: axc/invalid source callsign - %s\n", from);
		writelog(0,buffer);
	}

/*to call*/
	if (ax25_aton_arglist((const char**)to, &axconnect) == -1) {
		sprintf(buffer, "ERROR: axc/invalid destination callsign or digipeater\n");
		writelog(0,buffer);
	}

	/*
	 * Open the socket into the kernel.
	 */
	if ((fd = socket(AF_AX25, SOCK_SEQPACKET, 0)) < 0) {
		sprintf(buffer, "ERROR: axc/cannot open AX.25 socket, %s\n", strerror(errno));
		writelog(0,buffer);
	} else {
	    writelog(6,"========> Open fd %i in ax25connect",fd);
	}
	/*
	 * Set our AX.25 callsign and AX.25 port callsign accordingly.
	 */
	if (bind(fd, (struct sockaddr *)&axbind, addrlen) != 0) {
		sprintf(buffer, "ERROR: axc/cannot bind AX.25 socket, %s\n", strerror(errno));
		writelog(0,buffer);
	}

	flags = fcntl (fd, F_GETFL, 0);
	fcntl (fd, F_SETFL, flags | O_NONBLOCK);	// set the socket as nonblocking IO


	writelog(5,"   --> Connecting to %s (fd %i)...", to[0],fd);
	/*
	 * Lets try and connect to the far end.
	 */
	if (connect(fd, (struct sockaddr *)&axconnect, addrlen) != 0) {
		err=errno;
		switch (err) {
			case EINPROGRESS:
				writelog(3,"ONAIR: start AX25 connection from %s to %s",from,to[0]);
				break;
			case ECONNREFUSED:
				writelog(0,"ERROR: axc/Connection refused - aborting");
				close(fd); fd=-err;
				break;
			case ENETUNREACH:
				writelog(0,"ERROR: axc/No known route - aborting");
				close(fd); fd=-err;
				break;
			case EINTR:
				writelog(0,"ERROR: axc/Connection timed out - aborting");
				close(fd); fd=-err;
				break;
			default:
				writelog(0,"ERROR: axc/cannot connect to AX.25 callsign, %s", strerror(err));
				close(fd); fd=-err;
				break;
		}
	}
	
	return fd;
}

