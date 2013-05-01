/*
 *  agw.c
 *
 *  Copyright (C) 2006, Lieven De Samblanx, ON7LDS. All rights reserved.
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


#include "agw.h"
#include "utils.h"
#include "ax25connect.h"
#include "ldsped.h"


#define MAX_TOKS 10
#define DELIMITERS " \t"

int parseString(char *line, char *arrv[]) {
    int arrc;

    for (arrc=0;arrc<10;arrc++)arrv[arrc]=NULL;
    arrc = 0;
    arrv[arrc++] = strtok(line, DELIMITERS);
    writelog(5,"   ---> no path, connect directly to station %s",arrv[arrc-1]);
    while (((arrv[arrc] = strtok(NULL, DELIMITERS)) != NULL) && (arrc < MAX_TOKS)) ++arrc;

    return arrc;
}


int parse2String(char *line, char *to, char *arrv[]) {
    char* p;
    int arrc;

    writelog(5,"   ---> searching %i via stations.",line[0]);
    for (arrc=0;arrc<10;arrc++)arrv[arrc]=NULL;
    arrc = 0;
    p=line+1;
    arrv[arrc]=to;
    for (arrc=0;arrc<=line[0];arrc++)
        {
	    arrv[arrc+1]=p; p+=10;
	    if (arrc>0)
	        writelog(5,"   ----> found via station %s.",arrv[arrc]);
	}
    return arrc-1;
}


int processframe(char *framedata, int size, int conn_index, int monsock) {
    AGWFrame answer, frame;
    char s1[1060], s2[1024];
    char *s0;
    char *to_via[10],*to_via2[10];
    char *portdev;
    char *portname;
    char data[2000];
    int s, processed = 0, i=0;
    struct sockaddr sockfrom, sockto;
    socklen_t sockfromlen, socktolen;
    char zeroes[36];


    for (i=0;i<36;i++) zeroes[i]=0;

    //reservation of mem for C and v frame
    for(i=0; i<10; ++i )
        {
        to_via[i] = malloc(16*sizeof(char));  /* 15 characters + '\0' */
        to_via2[i] = to_via[i];
        if (to_via[i] == NULL)
    	    { writelog(0, "ERROR : Could not get necessary memory."); }
        }

    memcpy(&frame,framedata,35);

    i=3; if (frame.DataKind=='Y') i=4;
    writelog(i, "Connection %i: %c frame received.  Port %d, PID %02X, From <%s>, To <%s> Data length %i", 
	    connection[conn_index].nr,frame.DataKind, frame.Port , (unsigned char)frame.PID, frame.CallFrom, frame.CallTo, frame.DataLen);

    if (frame.Port>=ports) {
	writelog(0,"ERROR : port %i does not exist.",frame.Port);
	// not good. no further processing.
	frame.DataKind=0;
	processed=2;
	}

    if (frame.DataLen > 0) {
      if (frame.DataLen < sizeof(data) ) {
	  sockfromlen=sizeof(sockfrom);
	  size = recvfrom(connection[conn_index].fd, data, frame.DataLen, 0, &sockfrom, &sockfromlen);
	  if (size < 0) {
	     writelog(0, "ERROR : could not read data from connection %i (%m).", conn_index);
	    }
	  if (logging>2) {
		sprintf(s1," (expect %i|read %i)",frame.DataLen,size); 	
		if (size>0) 
		    { writestring(data,size,s2,sizeof(s2)); strcat(s1,s2); writelog(4, "%s",s2); }
		else 
		    { writelog(4, "%s",s1); }
	  }
      } else {
        writelog(0, "FATAL : to much data in frame for my humble memoryspace. Aborting connection."); 
        close_connection(conn_index);
      }
    }

//-------------------------------------------- Login required ?
    if ((connection[conn_index].mode<0) && (frame.DataKind != 'P')) {
	writelog(1, "No P frame although logging in is required. Not honoring your question.");
	// done :
	bcopy(zeroes,&frame,36);
	processed=2;
	}


//-------------------------------------------- R Frame
    if (frame.DataKind == 'R') {
	processed = 1;
	bcopy(zeroes,&answer,36);
	answer.DataKind = 'R';
	answer.DataLen=8;
	bcopy(&answer,s1,36);
    //	version = 2006.0401 :-))
	sprintf(s2, "%c%c%c%c%c%c%c%c", 0xd6, 0x07, 0x00, 0x00, 0x91, 0x01, 0x00, 0x00);
	for (i=0;i<8;i++) { s1[36+i]=s2[i]; };
	write_buffer(connection[conn_index].fd,s1,44);
	writelog(5, "   --> version sent");
	};


//-------------------------------------------- G Frame
    if (frame.DataKind == 'G') {
	processed = 1;
	bcopy(zeroes,&answer,36);
	answer.DataKind = 'G';

	sprintf(s2,"%i;",ports);
	portdev = NULL;
	if (ports)
	    for (i=1; i<=ports; i++) {
		portdev = ax25_config_get_next(portdev);
    		if ((portname = ax25_config_get_name(portdev)) == NULL)
        	    portname = portdev;
        	// agw does  not permit a ; in the description string
        	// we'll substitute it by a ,
        	strcpy(data,ax25_config_get_desc(portdev));
        	if (subst_char(data,';',',')) 
        	    writelog(2,"WARNING: A ; in the port description string is not allowed. Replacing by a ,");
		sprintf(s1,"Port%i %s (%s) - %s;",i, portname,
						     ax25_config_get_addr(portdev),
						     data);
		//some programs (i.e. PACLINK count on the exact stirng layout as agw seems to use
		// -> Portx with XXXXX on COMX: xxxxxxxxxxxx
		// -> Portx with XXXXX on LPTX: xxxxxxxxxxxx
		if (PORTINFO_STRICT)
		    sprintf(s1,"Port%i with TNC on Linux: %s (%s) - %s;",i, portname,
						     ax25_config_get_addr(portdev),
						     data);
		// check if portinfo does not go out of string bounds.
		// If yes : replace port desc by '?'. Hopefully not more than 20 times (=40 bytes : '?;').
		// When this happens, the G data is not agw complient any more for these ports ! (no 'Port:' string)
		if (strlen(s2)+strlen(s1) > (sizeof(s2)-40-21))
		    {
		     writelog(0,"ERROR: Total port information data exceeds foreseen memory space.");
		     strcpy(s1,"?");
		    }
		strcat(s2,s1);
	    }
	// agwpe seems to leave 20 bytes garbage after the null terminated string.
	// since Paclink seems to drop the conection after receiving the G frame
	// we now also put 20 bytes extra.
	strcat(s2,"--THIS--IS--GARBAGE--");
	answer.DataLen=strlen(s2);
	bcopy(&answer,s1,36);
	for (i=0;i<strlen(s2);i++) { s1[36+i]=s2[i]; };
	s1[36+i-21]=0;
	s1[36+i-1]=0;
	write_buffer(connection[conn_index].fd,s1,36+strlen(s2));
	writelog(5, "   --> portinfo sent");
	};

//-------------------------------------------- g Frame
    if (frame.DataKind == 'g') {
	processed = 1;
	bcopy(zeroes,&answer,36);
	answer.DataKind = 'g';
	answer.Port=frame.Port;

	if ( (ports>0) && (frame.Port>=0) && (frame.Port<=ports) ) {
	// ----- Baudrate
	    switch (portlist[frame.Port].baud) {
		case 1200  : s2[0]=0; break;
		case 2400  : s2[0]=1; break;
		case 9600  : s2[0]=2; break;
		case 19200 : s2[0]=3; break;
		default : s2[0]=255;
		}
	// Traffic level : 0xFF = no auto-update
	    s2[1]=255;
	//TX delay
	    s2[2]=ax_txdelay;
	//TX tail
	    s2[3]=ax_txtail;
	//Persist
	    s2[4]=ax_persist;
	//SlotTime
	    s2[5]=ax_slottime;
	//MaxFrame
	    s2[6]=ax_maxframe;
	//How Many connections are active on this port
	    s2[7]=0;
	    for (i=0;i<registration_conns;i++) 
		{ if (registration_conn[i].portnr==frame.Port) s2[7]++; }
	//How Many Bytes (received in the last 2 minutes) as a 32 bits (4 bytes) integer
	    s2[ 8]=0;
	    s2[ 9]=0;
	    s2[10]=0;
	    s2[11]=0;
	    // volgende regel eventueel nog nazien
	    i=portlist[frame.Port].bytecount2m;
	    bcopy(&i,&s2[8],4);

	    answer.DataLen=12;
	    bcopy(&answer,s1,36);
	    for (i=0;i<12;i++) { s1[36+i]=s2[i]; };
	    write_buffer(connection[conn_index].fd,s1,36+12);
	    writelog(5, "   --> port capabilities sent");
	    }
	    else writelog(2, "WARNING: port capabilities of nonexisten port (%i) cannot be sent",frame.Port);
	}

//-------------------------------------------- k Frame
    if (frame.DataKind == 'k') {

	processed = 1;
	if (connection[conn_index].mode>=0)
		connection[conn_index].mode = (connection[conn_index].mode ^ KFRAMES);
	
	writelog(2, "STATUS: Raw data frame sending for connection %i now is %i",
				conn_index, (connection[conn_index].mode & KFRAMES) / KFRAMES);
	};


//-------------------------------------------- m Frame
    if (frame.DataKind == 'm') {

	processed = 1;

	if (connection[conn_index].mode>=0)
		connection[conn_index].mode = (connection[conn_index].mode ^ MFRAMES);
	
	writelog(2, "STATUS: Monitor frame sending for connection %i now is %i",
			conn_index, (connection[conn_index].mode & MFRAMES) / MFRAMES);	
	};


//-------------------------------------------- K Frame
//  send data to AX port
    if (frame.DataKind == 'K'){
	processed = 1;
	if (connection[conn_index].mode & READWRITE) {
	    sockto.sa_family = AF_AX25;
	    strcpy(sockto.sa_data,portlist[frame.Port].dev);
	    socktolen=sizeof(sockto);
	    packets_mrtg_o++;

	    writelog(3, "ONAIR: sending data for connection %i via %s length %i",
			    connection[conn_index].nr, portlist[frame.Port].dev,socktolen);

	    if ((s = socket(AF_INET, SOCK_PACKET, htons(ETH_P_AX25))) == -1) {
		    writelog(0,"socket error while trying to send data via %s (%s)",
						    portlist[frame.Port].dev,strerror(errno));
	    } else {

		writelog(6,"========> Open fd %i in agwframe K",s);

	        if (sendto(s, data, size, 0, &sockto, socktolen) == -1) {
		    writelog(0, "ERROR: sendto error while trying to send data via %s (%m)"
						    ,portlist[frame.Port].dev);
		}
		if (close(s)<0)
		    writelog(0, "ERROR: pf/closing socket, %s",strerror(errno));
	    }
	} else {
	    writelog(1, "ERROR : sending data via this connection is not allowed");
	}
    }

//-------------------------------------------- H Frame
    if (frame.DataKind == 'H') {
	if (Hframes)
		mheard(frame.Port,conn_index);
	    else
		writelog(1, "As you know, I cannot answer H frames.");
	processed=1;
    };


//-------------------------------------------- P Frame
    if (frame.DataKind == 'P') {
	if (connection[conn_index].mode<0) {
	    strcpy(s1,data);     // call in s1
	    strcat(s1," ");
	    strcat(s1,data+255); // password added
	    for (i=0; i<logins; i++) {
		if (strcmp(login[i],s1)==0) { processed=1; break; }
	    }
	    if (! processed) {
		writelog(1, "ERROR : No legal login detected.");
		// done processing
		bcopy(zeroes,&frame,36);
		processed=2;
	    } else { 
		connection[conn_index].mode = abs(connection[conn_index].mode);
		s0 = strchr(s1,' '); s0[0]='\0';
		writelog(1, "Login from %s. (connection %i modestatus %i)",s1,conn_index,connection[conn_index].mode);
	    }
	} else {
	    processed=1;
	    writelog(1, "OOPS: Login and password received, but this is not necessary.");
	    writelog(1, "      I'm in a good mood and will let it go at that.");
	}
    }

//-------------------------------------------- M Frame
    if (frame.DataKind == 'M') {
        if (connection[conn_index].mode & READWRITE) {
		writelog(3,"ONAIR: UI frame to %s",frame.CallTo);
		sendbeacon(frame,data,size,NULL);
	} else {
	    writelog(1, "ERROR : sending data via this connection is not allowed");
	}
// niet goed:	send_frame('T', connection[conn_index].fd,NULL,data,size);
// T frame moet trouwens gedecodeerd zijn.
	processed=1;
    };



//-------------------------------------------- V Frame
    if (frame.DataKind == 'V') {
	if (size>0) {
	    strcpy(s1,frame.CallTo);
	    strcat(s1," VIA");
	    for (i=0;i<data[0];i++) {
		strcat(s1," ");
		strcat(s1,&data[1+10*i]);
	    }
	    if (connection[conn_index].mode & READWRITE) {
		    writelog(3,"ONAIR: UI frame to %s",s1);
		    sendbeacon(frame,&data[1+10*i],size-(1+10*i),s1);
	    } else {
		    writelog(1, "ERROR : sending data via this connection is not allowed");
	    }
	processed=1;
	} else {
	writelog(0,"ERROR: V frame with no data");
	};
    };


//-------------------------------------------- X Frame
    if (frame.DataKind == 'X') {
	processed = 1;
	bcopy(zeroes,&answer,36);
	strcpy(answer.CallFrom,frame.CallFrom);
	answer.DataKind = 'X';
	answer.DataLen = 1;
	bcopy(&answer,s1,36);
	if (connection[conn_index].mode & READWRITE) {
	    s1[36] = RegisterCall(frame.CallFrom,connection[conn_index].fd);
	} else {
	    s1[36] = 0;
	    writelog(1, "ERROR : No use in registering call, since sending data via this connection is not allowed");
	}
	write_buffer(connection[conn_index].fd,s1,37);
	writelog(5, "   --> registrationresult %i sent",s1[36]);
    };


//-------------------------------------------- x Frame
    if (frame.DataKind == 'x') {
	processed = 1;
	answer=frame;
	answer.DataLen = 1;
	bcopy(&answer,s1,36);
	s1[36] = UnRegisterCall(frame.CallFrom);
//	write_buffer(connection[conn_index].fd,s1,37);
//	writelog(5, "   --> un-registrationresult %i sent",s1[36]);
    };


//-------------------------------------------- C Frame
    if (frame.DataKind == 'C')  {
	processed = 1;
	answer=frame;
	answer.DataLen = 0;
	bcopy(&answer,s1,36);
	if (connection[conn_index].mode & READWRITE) {
	    if (FindRegisteredCall(frame.CallFrom)>=0) {
		writelog(5, "   --> trying direct connection from %s to %s.",frame.CallFrom,frame.CallTo);
		i = parseString(frame.CallTo,to_via);
		i = ax25connect(portlist[frame.Port].name, frame.CallFrom, to_via);
		if (i>=0) add_ax25connection(i,connection[conn_index].fd,frame.Port,frame.CallFrom,frame.CallTo);
	    } else { writelog(1, "ERROR : Connecting without registering call first is not possible"); }
	} else {
	    writelog(1, "ERROR : sending data via this connection is not allowed");
	    //send disconnect message to application
	    //because RO sockets are not supported by agwpe 
	    // and nevertheless we want to inform the application about not connecting
	    sprintf(s2,"*** DISCONNECTED From Station %s\r*",answer.CallFrom);
	    answer.DataLen=strlen(s2);
	    bcopy(&answer,s1,36);
	    for (i=0;i<strlen(s2);i++) { s1[36+i]=s2[i]; };
	        s1[36+i-1]=0;
	    write_buffer(connection[conn_index].fd,s1,36+strlen(s2));
	}
    };


//-------------------------------------------- v Frame
    if (frame.DataKind == 'v')  {
	processed = 1;
	answer=frame;
	answer.DataLen = 0;
	bcopy(&answer,s1,36);
	if ((data[0]<0) || (data[0]>7)) {
	    writelog(0,"ERROR: Number of VIA stations (%i) not allowed",data[0]);
	} else {
	    if (connection[conn_index].mode & READWRITE) {
	        if (FindRegisteredCall(frame.CallFrom)>=0) {
		    i = parse2String(data,frame.CallTo,to_via);
		    writelog(5, "   --> trying connection from %s to %s via %i stations.",
		    		frame.CallFrom,frame.CallTo,i);
		    i = ax25connect(portlist[frame.Port].name, frame.CallFrom, to_via);
		    if (i>=0) add_ax25connection(i,connection[conn_index].fd,frame.Port,frame.CallFrom,frame.CallTo);
		} else { writelog(1, "ERROR : Connecting without registering call first is not possible"); }
	    } else {
		writelog(1, "ERROR : sending data via this connection is not allowed");
		//send disconnect message to application
		//because RO sockets are not supported by agwpe 
		// and nevertheless we want to inform the application about not connecting
		sprintf(s2,"*** DISCONNECTED From Station %s\r*",answer.CallFrom);
		answer.DataLen=strlen(s2);
		bcopy(&answer,s1,36);
		for (i=0;i<strlen(s2);i++) { s1[36+i]=s2[i]; };
		    s1[36+i-1]=0;
		write_buffer(connection[conn_index].fd,s1,36+strlen(s2));
	    }
	}
    };


//-------------------------------------------- d Frame
    if (frame.DataKind == 'd')  {
	processed = 1;
	answer=frame;
	answer.DataLen = 0;
	bcopy(&answer,s1,36);
	writelog(5, "   --> disconnect AX25 connection from %s to %s.",frame.CallFrom,frame.CallTo);
	i=FindRegisteredCall(frame.CallFrom);
	if (i<0) {
	    // call not registered. do nothing.
	    writelog(0, "ERROR: Call not registered. Cannot disconnect %s -> %s.",frame.CallFrom,frame.CallTo);
	} else {
	    i=find_ax25connection(frame.CallFrom,frame.CallTo);
	    if (i<0) {
	        writelog(2, "INFO: connection from %s to %s already disconnected.",frame.CallFrom,frame.CallTo);
	        //send disconnect message to application although connection already is disconnected
	        //because agwpe does so (with the callfrom call !)
	        sprintf(s2,"*** DISCONNECTED From Station %s\r*",answer.CallFrom);
	        answer.DataLen=strlen(s2);
	        bcopy(&answer,s1,36);
	        for (i=0;i<strlen(s2);i++) { s1[36+i]=s2[i]; };
	            s1[36+i-1]=0;
	        write_buffer(connection[conn_index].fd,s1,36+strlen(s2));
	    } else {
	        writelog(2, "Closing AX25 connection %i from %s to %s (%i left)",
	            i+1,registration_conn[i].from, registration_conn[i].to, registration_conns-1);
	        ax25disconnect(i,1);
	    }
	}
    };


//-------------------------------------------- D Frame
    if (frame.DataKind == 'D') {
        if (connection[conn_index].mode & READWRITE) {
//TODO : check if connection is established before trying to send data
		writelog(3,"ONAIR: AX25 data frame to %s",frame.CallTo);
		write_ax25data(registration_conn[0].ax, data, size);
	} else {
	    writelog(1, "ERROR : sending data via this connection is not allowed");
	}
	processed=1;
    };


//-------------------------------------------- Y Frame
//-------------------------------------------- y Frame
/*
    There seems no way to detect the outstanding number of frames from the kernel.
    One possibility to get Y and y information, is to lower the kernel packet buffer
    so that only one packet kan be handed over to the kernel at a time. Then create
    a queue in ldsped to know what is outstanding.
    Another solution is counting the packets handed over to the kernel and check
    on the monitor socket what has been send (if the monitor socket shows what
    has been sent and not what has been given to the kernel -> I'll have to check
    this)
*/


/*
    Thanks to a suggention from NS7C, we'll take the outstanding bytes
    (not acknowledged) as found in /proc/net/ax25 to get an answer for this frame
*/
    if (frame.DataKind == 'Y')  {
	processed = 1;
	answer=frame;

	i=search_outstanding_bytes(frame.CallFrom,frame.CallTo);
	if (i<0) {
	    writelog(1,"ERROR: got bad response concerning outstanding frames");
	    i=0;
	}
	answer.DataLen = 4;
	bcopy(&i,&s2[0],4);
	
	bcopy(&answer,s1,36);
	for (i=0;i<4;i++) { s1[36+i]=s2[i]; };
	write_buffer(connection[conn_index].fd,s1,36+answer.DataLen);
	writelog(5, "   --> outstanding frames for connection sent");
    }
//-------------------------------------------- T Frame
/*
    While the docs say this is a way of knowing which frames have been sent,
    tests seem to show agwpe to answer with T frames immediately after getting
    the frame that has to been send.
    If this is really the case, I could send T frames the same way. If someone
    needs them. And if I know nobody who does.
*/


//-------------------------------------------- Unknown frame
    if (! processed) {
	if ( (frame.DataKind>32) && (frame.DataKind<126) )
	    writelog(2, "ERROR : Unknown frame (%c). Not processed.",frame.DataKind);
	else
	    writelog(2, "ERROR : Unknown frame (0x%02X). Not processed.",frame.DataKind);
    }


//-------------------------------------------- Free Used Mem
    for(i=0; i<10; ++i )
    {
        if (to_via2[i] != NULL) { free(to_via2[i]);}
    }


//--------------------------------------------
//    return processed;
    return 1;
};


int send_frame(char frametype, int writesock, char *port, char *data, int size, char *CFrom, char *CTo)
{
    AGWFrame frame;
    char s1[1060];
    int i=0;
    char zeroes[36];

    for (i=0;i<36;i++) zeroes[i]=0;
    bcopy(zeroes,&frame,36);

    /*search port dev list */
    if (port != NULL)
        for (i=0; i<ports; i++)
            if (strcmp(portlist[i].dev,port)==0) break;
    /* port not found ? : search port name list */
    if ( (port != NULL) && (i==ports) )
        for (i=0; i<ports; i++)
            if (strcmp(portlist[i].name,port)==0) break;

    if (i==ports)
    {
	writelog(0, "ERROR : send_frame got unknown portname (%s) -- assuming port 0 (%s)",
		    port,portlist[0].name);
	i=0;
    }


    frame.Port = i;
    frame.res1 = 0;
    frame.res2 = 0;
    frame.res3 = 0;
    frame.DataKind = frametype;
    frame.res4 = 0;
    frame.PID = 0;
    frame.res5 = 0;
    if (strlen(CFrom)==0)
	{
	    sprintf(frame.CallFrom,"LDSPED");
	    sprintf(frame.CallTo,  "LDSPED");
	}
    else
	{
	    strncpy(frame.CallFrom,CFrom,10);
	    strncpy(frame.CallTo  ,CTo  ,10);
	}
    frame.DataLen = size;
    frame.res6 = 0;
    bcopy(&frame,s1,36);

    for (i=0;i<size;i++) { s1[36+i]=data[i]; };
    i = write_buffer(writesock,s1,36+size);
    writelog(5, "   --> frame %c, %i bytes data seen on port %i and sent : resultcode %i",
			    frametype, size, frame.Port, i);

    return i;
};


int RegisterCall(char *buf, int fd)
{
    char call[15];
    int i;

    if (registrations<MAX_REGISTRATION_CALLS) {
	if (strlen(buf)==0) {
	    writelog(0, "ERROR: Call registration frame not correct (%s).",buf);
	    return 0;
	} else {
	strncpy(call,buf,10); call[11]=0;
	if (registrations)
	    for (i=0;i<registrations;i++)
		{ if ( (strstr(registration_call[i].call,call)!=NULL) 
			&& (strlen(registration_call[i].call) == strlen(call)) )
		    {writelog(1, "Call %s already registered",call);  return 0; } 
		}
	strcpy(registration_call[registrations].call,call);
	registration_call[registrations].tcp_fd=fd;
	registrations++;
	writelog(2, "Registered call %s (%i total)",call,registrations);
	return 1;
    }
	} else {
	writelog(0, "ERROR: Maximum registered calls (%i) reached.",MAX_REGISTRATION_CALLS);
	return 0;
	}
}



void UnRegisterCalls(int fd)
{
    int i,j;

    i=0;
    if (fd>=0) {
	while (i<registrations)
	{
	    if (registration_call[i].tcp_fd==fd) {
		    registrations--;
		    close_ax25connections(registration_call[i].tcp_fd);
		    writelog(2, "Unregistered call %s (%i left).",registration_call[i].call,registrations);
		    for (j=i;j<registrations;j++) {
			strcpy(registration_call[j].call,registration_call[j+1].call);
			registration_call[j].tcp_fd=registration_call[j+1].tcp_fd;
		    }
	    } else i++;
	}
    }
}




int UnRegisterCall(char *buf)
{
    char call[15];
    int i,callnr = -1;

/*    for (i=0;i<registrations;i++) {
	writelog(5,"    -#-> Registration %i : fd %i : call %s",i,registration_call[i].tcp_fd,registration_call[i].call);
    }
*/

    if (registrations>0) {
	if (buf != NULL) {
	    strncpy(call,buf,10); call[11]=0;
	    for (i=0;i<registrations;i++)
	        { if (strstr(registration_call[i].call,call)!=NULL) {callnr=i;} }
	}
	if (callnr<0) {
	    writelog(0, "ERROR: Call unregistration not possible (%s).",buf);
	    return 0;
	}

	if (callnr>=0) {
	    registrations--;
//	    writelog(5,"    -CLOSE-> Registration %i : fd %i : call %s",i,registration_call[callnr].tcp_fd,registration_call[callnr].call);
	    close_ax25connections(registration_call[callnr].tcp_fd);
	    for (i=callnr;i<registrations;i++)
	        {
	        strcpy(registration_call[i].call,registration_call[i+1].call);
	        registration_call[i].tcp_fd=registration_call[i+1].tcp_fd;
	        }
	    writelog(2, "Unregistered call %s (%i left).",call,registrations);
	    } else {
	        writelog(1, "Unregistering call %s not possible (call not registered)",call);
	        return 0;
	}
	return 1;
	} else {
	writelog(0, "ERROR: No registered calls, can not unregister.");
	return 0;
	}
}


int FindRegisteredCall(char *buf)
{
    char call[15];
    int i,callnr = -1;

    if (registrations>0) {
	if (buf != NULL) {
	    strncpy(call,buf,10); call[11]=0;
	    for (i=0;i<registrations;i++)
	        { if ( (strstr(registration_call[i].call,call)!=NULL) 
			&& (strlen(registration_call[i].call) == strlen(call)) )
		    {callnr=i;} }
	}
    }
//    writelog(5,"Call %s is at %i",buf,callnr);
    return callnr;
}
