/*
 *  utils.c - agwpe linux replacement
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



#include <time.h>
#include <stdarg.h>


#include "ldsped.h"
#include "utils.h"
#include "aprs.h"
#include "agw.h"

int packets,  // packets_mrtg_i,packets_mrtg_o : see utils.h
    trafficsperhour, trafficsendport, 
    probesendport, probesperhour;
time_t t1_traf,t2_traf,
	t1_probe,t2_probe;

//-----------------------------------------------------------------

void writechar(char C, char *buf){
  if ( (C<31) || (C==127) ) { sprintf(buf, "[%x]",(C & 0xff) ); }
    else sprintf(buf, "%c",C);
};

void writestring(char *buf, int size, char *wstring, int wsize) {
int i;
char buffer[10];

    strcpy(wstring,"");
  if (size>0) { 
	for (i=0; i<size; i++) {
	    writechar(buf[i],buffer);
	    strcat(wstring,buffer);
	    if ( (strlen(wstring)+10) >=wsize)
	      { strcat(wstring,"..."); break; }
	};
    }
}


int write_buffer(int fd, char *buf, int count) {
  int  status = 0, result;

  if (count < 0) return (-1);
 
  while (status != count) {
    result = write(fd, buf + status, count - status);
    if (result < 0) return result;
    status += result;
  }
  return (status);
}



void writelog(int level, char *fmt, ...)
{
    va_list args;
    char str[1024];
    unsigned char loglevel=LOG_INFO;
    time_t timenowx;
    struct tm *timenow;

    va_start(args, fmt);
    vsnprintf(str, 1024, fmt, args);
    va_end(args);

    if (level==0)
	loglevel=LOG_ERR;

    if (logging>=level) {
	if ( become_daemon )
	    { str[98]='.';str[99]='.';str[100]='.'; str[101]=0; syslog(loglevel, str); }
	else
	    { time(&timenowx);
	      timenow = localtime(&timenowx);
	      printf("[%02d:%02d:%02d] ", timenow->tm_hour, timenow->tm_min, timenow->tm_sec);
	      printf("%s\n",str);
	    }
    }
}


int subst_char(char *s1, int ch, int ch2) {

    int count = 0; 
    char *wrk = strchr(s1, ch);

    while (wrk) {
	*wrk = (char) ch2;
	count++, wrk++;
	wrk = strchr(wrk, ch);
    }
    return count;
}


void clear_stats(void)
{
  time(&t1_traf);      
  t2_traf=t1_traf;
  t1_probe=t1_traf+7; 
  t2_probe=t1_probe;
  packets = 1;
  packets_mrtg_i = 1;
  packets_mrtg_o = 0;
  trafficsperhour = 0;
}



void init_station(char *buf,int type)
{
    char tmp[75];
    char *p;
    
    switch (type) {
     case 0 :
	    sprintf(station_text,STATIONTEXT,LDSPED_VERSION);
	    break;
     case 1 :
	    strncpy(station_text,buf,70);
	    p=strstr(station_text,"$V");
	    if (p != NULL)
		{
		    strcpy(tmp,p+2);
		    sprintf(p,"ldsped V%s",LDSPED_VERSION);
		    strcat(station_text,tmp);
		} else {
		    p=station_text;
		}
	    if ( (strlen(station_text)<2) || (strlen(station_text)>67 ) ) {
		writelog(0,"WARNING: stationtext to short or to long. Switching to default.");
		sprintf(p,STATIONTEXT,LDSPED_VERSION);
	    }
	    writelog(1,"Station status text = <%s>",station_text); 
	    break;
    }
}


void init_traffic(char *buf,int type)
{
    switch (type) {
     case 0 :
    	    trafficsendport = 255;
	    trafficsperhour = 0;
	    trafficperiod = 0;
	    trafficpos[0]=0;
	    strcpy(trafficname,"TRAFMON  ");
	    strcpy(traffic_unproto,"WIDE2-2");
	    break;
     case 1 :
	    trafficsperhour=atoi(buf);
	    if ((trafficsperhour>=1) & (trafficsperhour<=10)) {
	        trafficperiod = 3600 / trafficsperhour;
	    } else {
		writelog(0,"WARNING: traffic period not valid. Disabeling.");
		trafficperiod=0;
	    }
	    writelog(1,"Traffic stats per hour = %i -> period = %i sec",trafficsperhour,trafficperiod);  
	    break;
     case 2 :
    	    trafficsendport=atoi(buf);
	    if ( (trafficsendport>=0) & (trafficsendport>=ports) ) {
		writelog(0,"ERROR: traffic send port (%i) does not exist.",trafficsendport);
		trafficsendport=-1;
		}
	    if (trafficsendport<0) trafficsendport=255;	    	
	    break;
    case 3 :
            if ( strlen(buf)==19 ) {
                    strcpy(trafficpos,buf);
                    writelog(1,"Traffic position <%s>",trafficpos);
            }
	    break;
    case 4 :
    	    if ( strlen(buf)<10 ) { 
		    strcpy(trafficcall,buf);
		    writelog(1,"Traffic originating call = <%s>",trafficcall);
		}
	    if ( strlen(trafficcall) == 0 ) {
		    writelog(1,"Traffic originating call is call of the sending port.");
		}
	    break;	
    case 5 :
    	    if ( strlen(buf)<10 ) { 
		    strcpy(trafficname,buf);
		    writelog(1,"Traffic object name = <%s>",trafficname); 
	    } else {
		writelog(0,"WARNING: traffic object name not valid. Switching to default.");
	    }
	    if ( strlen(trafficname) == 0 ) {
		    writelog(1,"Traffic object name = TRAFFIC");
		    strcpy(trafficname,"TRAFFIC");
		}
	    strncat(trafficname,"          ",9-strlen(trafficname));
	    trafficname[9]=0;
	    break;	
     case 6 :
    	    if (strlen(buf)<100) 
		strcpy(traffic_unproto,buf);
	    writelog(1,"Traffic unproto path = <%s>",traffic_unproto);
	    break;
    }
}


void init_probe(char *buf, int type)
{
    char *s;
    
    switch (type) {
     case 0 :
    	    strcpy(probe_unproto,"");
	    strcpy(phgr,""); 
	    strcpy(probecall,""); 
	    probesendport=255;
	    break;
     case 1 : //since we get the period out of the PHGR string, this is not necessary any more.
	    probesperhour=atoi(buf);
	    if ((probesperhour>=1) & (probesperhour<=10))
		{
		    probeperiod = 3600 / probesperhour;
		    writelog(1,"Probe stats per hour = %i   -> period = %i sec",probesperhour,probeperiod);  
		} else {
		    writelog(0,"WARNING: probe period not valid. Disabeling.");
		    probeperiod=0;
		}
	    break;
     case 2 :
    	    if (strlen(buf)<100) 
		strcpy(probe_unproto,buf);
	    writelog(1,"Probe unproto path = <%s>",probe_unproto);
	    break;
     case 3 :
	    // min. length 9 : 'PHGabcdr/'  see http://web.usna.navy.mil/~bruninga/aprs/probes.txt
	    if ( (strlen(buf)>9 ) & (strlen(buf)<100) ) {
		strcpy(phgr,buf);
		s = strchr(buf,'/'); 
		if (s != NULL) {
		    s--;
		    probesperhour=s[0]-48;
		    if (probesperhour>16) probesperhour-=7;
		    if ((probesperhour>=1) & (probesperhour<=35))
			{
			    probeperiod = 3600 / probesperhour;
			    writelog(1,"Probe stats per hour = %i   -> period = %i sec",probesperhour,probeperiod);  
			} else {
			    writelog(0,"WARNING: probe period not valid. Disabeling.");
			    probeperiod=0;
			}
		}
	    }
	    writelog(1,"PHGR string = <%s>",phgr);
	    break;
     case 4 :
    	    probesendport=atoi(buf);
	    if ( (probesendport>=0) & (probesendport>=ports) ) {
		writelog(0,"ERROR: probe send port (%i) does not exist.",probesendport);
		probesendport=-1;
		}
	    if (probesendport<0) probesendport=255;
	    break;
     case 5 :
    	    if ( strlen(buf)<10 ) { 
		    strcpy(probecall,buf);
		    writelog(1,"Probe originating call = <%s>",probecall);
		}
	    if ( strlen(probecall) == 0 ) {
		    writelog(1,"Probe originating call is call of the sending port.");
		}
	    break;
    }	
}



void init_axprms(char *buf, int type)
{
    switch (type) {
     case 0 :
	    ax_txdelay=atoi(buf);
	    if ( (ax_txdelay<0) || (ax_txdelay>255) )
		{ ax_txdelay=0; writelog(0,"ERROR: TX DELAY out of range. Assuming 0."); }
	    break;
     case 1 :
	    ax_txtail=atoi(buf);
	    if ( (ax_txtail<0) || (ax_txtail>255) )
		{ ax_txtail=0; writelog(0,"ERROR: TX TAIL out of range. Assuming 0."); }
	    break;
     case 2 :
	    ax_persist=atoi(buf);
	    if ( (ax_persist<0) || (ax_persist>255) )
		{ ax_persist=0; writelog(0,"ERROR: PERSIST out of range. Assuming 0."); }
	    break;
     case 3 :
	    ax_slottime=atoi(buf);
	    if ( (ax_slottime<0) || (ax_slottime>255) )
		{ ax_slottime=0; writelog(0,"ERROR: SLOTTIME out of range. Assuming 0."); }
	    break;
     case 4 :
	    ax_maxframe=atoi(buf);
	    if ( (ax_maxframe<0) || (ax_maxframe>7) )
		{ ax_maxframe=0; writelog(0,"ERROR: MAXFRAME out of range. Assuming 0."); }
	    break;
    }
}


void init_script(char *buf, int type)
{
    char* l;
    int i;

    switch (type) {
     case 0 :
	    scriptuser=atoi(buf);
	    if ( (scriptuser<0) || (scriptuser>65534) )
		{ scriptuser=65534; writelog(0,"ERROR: SCRIPTUSER out of range. Setting to 65534."); }
	    break;
     case 1 :
	    scriptgroup=atoi(buf);
	    if ( (scriptgroup<0) || (scriptgroup>65534) )
		{ scriptgroup=65534; writelog(0,"ERROR: SCRIPTGROUP out of range. Setting to 65534."); }
	    break;
     case 2 :
	    strncpy(scripts[scriptpaths].name,buf,10); scripts[scriptpaths].name[9]='\0';
	    for (i=0;i<10;i++) if (scripts[scriptpaths].name[i]==' ') {scripts[scriptpaths].name[i]='\0'; }
	    l=strstr(buf," "); 
	    if (l!=NULL) { l++; strncpy(scripts[scriptpaths].path,l,127); scripts[scriptpaths].path[126]='\0'; }
	    if ( (strlen(scripts[scriptpaths].name)>0) && (scripts[scriptpaths].path>0) ) {
		writelog(5,"Script %d command '%s' executes '%s'",scriptpaths,scripts[scriptpaths].name,scripts[scriptpaths].path);
		scriptpaths++;
	    }
	    break;
    }
}


int check_parameters(void) {
    if (strlen(trafficpos)<19) {
        writelog(0,"ERROR: no suitable position found (trafficpos parameter).");
        return -1;
        }
    if ((probeperiod != 0) && ( (strlen(phgr)<10) || (strstr(phgr,"PHG")==NULL)) ) {
        writelog(0,"ERROR: probe misconfiguration.");
        return -2;
        }


    return 0;
}


void check_probe(void) {
 int t;
 char s1[500], s2[100];
 AGWFrame frame;
 struct tm *timenow;

    if (probeperiod == 0) return; 
    frame.Port=probesendport;
    strcpy(frame.CallFrom,probecall);
    strcpy(frame.CallTo,APDEST);

    // not really necessary :
    frame.DataKind=76;
    frame.PID=0;
    frame.DataLen=0;
    //

    time(&t2_probe);
    t=t2_probe-t1_probe;
    if (t>=probeperiod-370)  {
	t1_probe=t2_probe;
	if (strlen(probecall)==0) {
	    /* nog niet goed */
	    sprintf(s1,";         *");
	} else {
	    sprintf(s1,";%s*",probecall);
	}

	timenow = localtime(&t2_traf);
	sprintf(s2,"%02d%02d%02dz%s", timenow->tm_mday, timenow->tm_hour, timenow->tm_min, trafficpos);
	strcat(s1,s2);
	strcat(s1,phgr);

	strcpy(s2,frame.CallTo);
	    strcat(s2," VIA ");
	    strcat(s2,traffic_unproto);

	sendbeacon(frame,s1,strlen(s1),s2);
	writelog(5,"   Probe send <%s> <%s>",s1,s2);
    }
}


int count_packets(int count)
{
 int t,p;
 char s1[500], s2[100];
 struct tm *timenow;
 AGWFrame frame;

    if (trafficperiod == 0) return packets; 

    frame.Port=255; // send on all ports
    strcpy(frame.CallFrom,trafficcall);
    strcpy(frame.CallTo,APDEST);

    // not really necessary :
    frame.DataKind=76;
    frame.PID=0;
    frame.DataLen=0;
    //
    time(&t2_traf);
    if (count) { packets++; packets_mrtg_i++; }
    t=t2_traf-t1_traf;
    if (t>=trafficperiod)  {
	t1_traf=t2_traf;

	sprintf(s1,";%s*",trafficname);

	timenow = localtime(&t2_traf);
	sprintf(s2,"%02d%02d%02dz%s", timenow->tm_mday, timenow->tm_hour, timenow->tm_min, trafficpos);
	strcat(s1,s2);
	p=packets*60; p=p/t;
	sprintf(s2," %3i packets in %2i minutes  (= %i/min)",packets, t/60, p);
	strcat(s1,s2);
	writelog(3,"ONAIR: TRAFFIC beacon (%i packets in %i minutes = %i/min)",packets, t/60, p);
	packets_mrtg_o++;

	strcpy(s2,frame.CallTo);
	    strcat(s2," VIA ");
	    strcat(s2,traffic_unproto);

	sendbeacon(frame,s1,strlen(s1),s2);

	writelog(4,"Measured %d sec, %i packets.",t , packets);
	packets=1;
    }
    return packets;
}


static int byte_count_timer()
{
    time_t t;

    t = time(NULL);
    if (t-byte_count_timer_val >= 120)
	{ byte_count_timer_val = t; return 1; }
    else
	{ return 0; }
}



void port_byte_count(int size,char *port)
{
 int i;

    if (size)
	{
	    for (i=0; i<ports; i++) {
	    if (strcmp(portlist[i].dev,port)==0) break;
	     }
	    portlist[i].bytecount+=size;

	}
    // 2 minutes elapsed ?
    if ( byte_count_timer() )
	for (i=0; i<ports; i++) {
	    portlist[i].bytecount2m=portlist[i].bytecount;
	    portlist[i].bytecount=0;
	}
    // writelog(5,"===%i bytes seen on port %i = %s",size,i,port);
}


void put_traffic_stats(int conn_index){

    char s[50];

    writelog(4,"Traffic status send : %i packets.",packets_mrtg_i);
    sprintf(s, "%i\n%i\nsome time\nldped traffic stats (%i %i)\n",
			packets_mrtg_i,packets_mrtg_o,packets_mrtg_i,packets_mrtg_o);
    packets_mrtg_i = 0;
    packets_mrtg_o = 0;
    write_buffer(connection[conn_index].fd,s,strlen(s));
}


void dump_info(void)
{
 int i;
 FILE *fp;
 char msg[20];

 writelog(0,"***************** INFO *****************");
 writelog(0,"ldsped version %s", LDSPED_VERSION);
 writelog(0,"Copyright (C) 2006-2010 ON7LDS. All rights reserved.");
 writelog(0,"-- Loglevel        : %i",logging);
 writelog(0,"-- Mheardd data file");

 if (Hframes) {
    writelog(0, "    When ldsped started, mheard data was available");
    writelog(0, "    H frames should be processed");
    } else {
    writelog(0, "    When ldsped started, mheard data was not available");
    writelog(0, "    H frames are not processed");
    }
 
 if ((fp = fopen(DATA_MHEARD_FILE, "r")) == NULL) {
    writelog(0, "    %s not present",DATA_MHEARD_FILE);
    } else {
    writelog(0, "    %s present",DATA_MHEARD_FILE);
    fclose(fp);
    }
 writelog(0,"-- Ports           : %i",ports);
 for (i=0;i<ports;i++) { 
    writelog(0, "    Port %i : %s (%s) %s",i, portlist[i].dev,
					     portlist[i].name,
					     portlist[i].call
					     );
    writelog(0, "         (%i bytes now, %i bytes last 2 min)",
					     portlist[i].bytecount,
					     portlist[i].bytecount2m
					     );
    }
 if (PORTINFO_STRICT)
	 writelog(0,"    Using compatible portinfo string");
    else writelog(0,"    Using ldsped portinfo string");
 writelog(0,"-- Sockets         : %i",sock_num);
 for (i=0;i<sock_num;i++)
    writelog(0,"    Socket %i : rw=%i",i,socks[i].mode);
 writelog(0,"-- TCP Connections : %i",conn_num);
 for (i=0;i<conn_num;i++) 
    writelog(0, "    Connection %i from %s:%i modestatus %i",
	connection[i].nr, 
	inet_ntoa(connection[i].addr.sin_addr),
	ntohs(connection[i].addr.sin_port),
	connection[i].mode);
 writelog(0,"-- Registered calls : %i",registrations);
 for (i=0;i<registrations;i++) 
    writelog(0, "    %s",
	registration_call[i].call);
 writelog(0,"-- AX25 Connections: %i",registration_conns);
 for (i=0;i<registration_conns;i++) 
    {
	if (registration_conn[i].connect==0)
	    strcpy(msg,"Connection");
	else
	    strcpy(msg,"Connecting");
        writelog(0, "    %s on port %i  : from %s to %s",
		msg,
		registration_conn[i].portnr,
		registration_conn[i].from,
		registration_conn[i].to
		);
    }

 writelog(0,"-- Traffic monitor");
 if (trafficsendport==255) 
    writelog(0,"    Sending on all %i port(s)",ports);
    else
    writelog(0,"    Sending on port %i",trafficsendport);
 writelog(0,"    Traffic stats per hour : %i",trafficsperhour);
 writelog(0,"    Trafficperiod          : %i s",trafficperiod);
 time(&t2_traf);
 writelog(0,"    Actual timelap         : %i s",(int)t2_traf-t1_traf);
 writelog(0,"    Actual packets         : %i",packets);
 writelog(0,"    Traffic object name    : %s",trafficname);
 writelog(0,"    Traffic object path    : %s",traffic_unproto);
 writelog(0,"    Trafficposition        : %s",trafficpos);
 writelog(0,"-- Probe :");
 if ( strlen(probecall) ==0 )
    writelog(0,"    Source port            : Portcall");
    else
    writelog(0,"    Source port            : %s",probecall);
 if (probesendport==255) 
    writelog(0,"    Sending on all %i ports",ports);
    else
    writelog(0,"    Sending on port %i",probecall);
 writelog(0,"    Probes per hour        : %i",probesperhour);
 writelog(0,"    Probeperiod            : %i s",probeperiod);
 time(&t2_probe);
 writelog(0,"    Actual timelap         : %i s",(int)t2_probe-t1_probe);
 writelog(0,"    PHGR                   : %s",phgr);
 writelog(0,"    Path                   : %s",probe_unproto);
 writelog(0,"-- APRSQ scripts :");
 writelog(0,"    Scripts defined        : %i",scriptpaths);
 writelog(0,"    Scripts running        : %i",scriptchilds);



 writelog(0,"*************** END INFO ***************");
}


void init_ax_config_calls()
{
 int  i;
 char *port, *portname;
 
//ax25_config_load_ports must be called once, but only once
    if (ports<0) {
	ports = ax25_config_load_ports();

	writelog(1, "AX25 Port list :");
        port=NULL;
	if (ports>0)
    	    for (i=0; i<ports; i++) {
		port = ax25_config_get_next(port);
		strcpy(portlist[i].dev,ax25_config_get_dev(port));
                if ((portname = ax25_config_get_name(port)) == NULL)
		                    portname = port;
		strcpy(portlist[i].name,portname);
		strcpy(portlist[i].call,ax25_config_get_addr(port));
		portlist[i].baud=ax25_config_get_baud(port);
		portlist[i].bytecount=0;
		portlist[i].bytecount2m=0;


	writelog(1, " port %i : %s (%s) %s %ibd",i, portlist[i].dev,
						 portlist[i].name,
						 portlist[i].call,
						 portlist[i].baud);

	} else { 
	    writelog(0, "ERROR : No AX25 ports seem to exist, so there is no use in running this program.");
	    writelog(0, "Exiting ldsped ...");
	    exit(1); 
	}
	writelog(1, "A total of %i port(s).",ports);
    }
}



void clear_lprintbuf()
{
 strcpy(lprintbuf," 0:");
}


int write_lprintbuf(int conn_nr, char *port)
{
 int i;
 char p[3];
 char FrCall[10],ToCall[10];
 char *s;
 char line[200];


 strcpy(FrCall,"?"); strcpy(ToCall,"?");

 for (i=0; i<ports; i++) {
    if (strcmp(portlist[i].dev,port)==0) break;
 }
 sprintf(p,"%2i",i);
 memcpy(lprintbuf,p,2);

 if (( s = strstr(lprintbuf,"Fm ")) != NULL)
    {
	strncpy(FrCall,s+3,10);
	if (( s = strstr(FrCall," ")) != NULL)
	     *s = '\0';
    }

 if (( s = strstr(lprintbuf,"To ")) != NULL)
    {
	strncpy(ToCall,s+3,10);
	if (( s = strstr(ToCall," ")) != NULL)
	     *s = '\0';
    }
 writelog(5,"   Preparing U frame for connection %i: stations : From [%s] to [%s].",connection[conn_nr].nr,FrCall,ToCall);

 for (i=0;i<strlen(lprintbuf);i++) { if (lprintbuf[i]==0xA) lprintbuf[i]= 0xD; };
 if (lprintbuf[strlen(lprintbuf)-1]!=0xD) { strcat(lprintbuf,"\r\0x00"); }

 strncpy(line,lprintbuf,199); line[199]=0; for (i=0;i<strlen(line);i++) { if (line[i]<32) line[i]= '|'; };
 writelog(6,"   ===> (%s)",line);

 i=send_frame('U', connection[conn_nr].fd, port, lprintbuf, strlen(lprintbuf)+1,FrCall,ToCall);

// strcpy(lprintbuf,"  :");

 return i;
}


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
//	fputs(str, stdout);
//	fflush(stdout);
	strcat(lprintbuf,str);
}


void data_dump(unsigned char *data, int length, int dumpstyle)
{
	unsigned char c;
	int i;
	int cr = 1;
	char buf[BUFSIZE];

	for (i = 0; length > 0; i++) {

		c = *data++;
		length--;

		switch (c) {
		case 0x00:
			buf[i] = ' ';
		case 0x0A:	/* hum... */
		case 0x0D:
			if (cr)
				buf[i] = '\n';
			else
				i--;
			break;
		default:
			buf[i] = c;
		}
		cr = (buf[i] != '\n');
	}
	if (cr)
		buf[i++] = '\n';
	buf[i++] = '\0';
	lprintf(T_DATA, "%s", buf);
}


void display_timestamp(void)
{
        time_t timenowx;
        struct tm *timenow;

        time(&timenowx);
        timenow = localtime(&timenowx);

        lprintf(T_TIMESTAMP, "[%02d:%02d:%02d]", timenow->tm_hour,
	                timenow->tm_min, timenow->tm_sec);
}



void arp_dump(unsigned char *data, int length)
{ lprintf(0,"ARP protocol decoding unsupported"); }

void netrom_dump(unsigned char *data, int length, int hexdump, int type)
{ lprintf(0,"netrom protocol decoding unsupported"); }

void ip_dump(unsigned char *data, int length, int hexdump)
{ lprintf(0,"IP protocol decoding unsupported"); }

void rose_dump(unsigned char *data, int length, int hexdump)
{ lprintf(0,"rose protocol decoding unsupported"); }

void flexnet_dump(unsigned char *data, int length, int hexdump)
{ lprintf(0,"flexnet protocol decoding unsupported"); }





FILE *proc_fopen(const char *name)
{
    static char *buffer;
    static size_t pagesz;
    FILE *fd = fopen(name, "r");

    if (fd == NULL)
      return NULL;

//	writelog(6,"========> Open fd ?? in proc_fopen");


    if (!buffer) {
      pagesz = getpagesize();
      buffer = malloc(pagesz);
    }

    setvbuf(fd, buffer, _IOFBF, pagesz);
    return fd;
}



int ax25_proc_info(void)
{
    FILE *f;
    char buffer[256], buf[16];
    char *src, *dst, *dev, *p;
    int st, vs, vr, sendq, recvq, ret;
    int new = -1;		/* flag for new (2.1.x) kernels */
    int count = 0;

    

    if (!(f = proc_fopen(_PATH_PROCNET_AX25))) {
	if (errno != ENOENT) {
	    writelog(0,"ERROR: api/%s (%s)",strerror(errno),_PATH_PROCNET_AX25);
	    writelog(0,"WARNING: stopping proc checks (not good for y frames).");
// ------------------------------------------- debug -------------------------------------------
//	    fclose(file1);
//	    fclose(file2);
//	    system("lsof > /tmp/openfiles");
//	    exit(1);
// ------------------------------------------- debug -------------------------------------------
	    return (0);
	}
    }
//	writelog(6,"========> Open fd ?? in ax25_proc_info");

	while (fgets(buffer, 256, f)) {
	if (new == -1) {
	    if (!strncmp(buffer, "dest_addr", 9)) {
		new = 0;
		continue;	/* old kernels have a header line */
	    } else
		new = 1;
	}
	/*
	 * In a network connection with no user socket the Snd-Q, Rcv-Q
	 * and Inode fields are empty in 2.0.x and '*' in 2.1.x
	 */
	sendq = 0;
	recvq = 0;
	if (new == 0) {
	    dst = buffer;
	    src = buffer + 10;
	    dst[9] = 0;
	    src[9] = 0;
	    ret = sscanf(buffer + 20, "%s %d %d %d %*d %*d/%*d %*d/%*d %*d/%*d %*d/%*d %*d/%*d %*d %*d %*d %d %d %*d",
		   buf, &st, &vs, &vr, &sendq, &recvq);
	    if (ret != 4 && ret != 6) {
		writelog(0,"ERROR: Problem reading data from %s", _PATH_PROCNET_AX25);
		continue;
	    }
	    dev = buf;
	} else {
	    p = buffer;
	    while (*p != ' ') p++;
	    p++;
	    dev = p;
	    while (*p != ' ') p++;
	    *p++ = 0;
	    src = p;
	    while (*p != ' ') p++;
	    *p++ = 0;
	    dst = p;
	    while (*p != ' ') p++;
	    *p++ = 0;
	    ret = sscanf(p, "%d %d %d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %d %d %*d",
		   &st, &vs, &vr, &sendq, &recvq);
	    if (ret != 3 && ret != 5) {
		    writelog(0,"ERROR: Problem reading data from %s\n", _PATH_PROCNET_AX25);
		    continue;
	    }
	    /*
	     * strip digipeaters
	     */
	    p = dst;
	    while (*p && *p != ',') p++;
	    *p = 0;
	}
	//put data in array
	if (count<MAX_PROC_CONNECTIONS) {
	    strcpy(procinfo_connections[count].src,src);
	    strcpy(procinfo_connections[count].dst,dst);
	    procinfo_connections[count].sendQ=sendq;
	    procinfo_connections[count].state=st;
	    count++;
	} else {
	    writelog(0,"ERROR: to much connections for my memory.");
	    writelog(0,"WARNING: stopping proc checks (not good for y frames).");
	    if (fclose(f)<0)
		writelog(0, "ERROR: api1/closing proc, %s",strerror(errno));
	    return 0;
	}
    }
    procinfo_connections[count].sendQ=-1;

    if (fclose(f)<0)
	writelog(0, "ERROR: api2/closing proc, %s",strerror(errno));
    return 1;
}


int search_outstanding_bytes(char *src, char *dst) {
    int count, sendq;

    sendq = -1;
    count = 0;

    while ( (procinfo_connections[count].sendQ>=0) && (sendq<0) ) {
	if (
		( strstr(procinfo_connections[count].src,src)!=NULL )
		&&
		( strstr(procinfo_connections[count].dst,dst)!=NULL )
	    ) {
	    sendq = (procinfo_connections[count].sendQ+50) / 300;
	    writelog(5,"   --> Connection %s->%s has %i bytes in queue (frameresult %i)",
				src,dst,procinfo_connections[count].sendQ,sendq);
	}
	count++;
    }
    if (sendq<0) writelog(1,"Warning: found no connection from %s to %s.",src,dst);

    return sendq;

}

int search_connection_status(char *src, char *dst) {
    int count,stat;
    static char *ax25_state[5] =
            { "LISTENING", "SABM SENT", "DISC SENT", "ESTABLISHED", "RECOVERY" };

    stat = -1;
    count = 0;

    while (stat<0)  {
	if (
		( strstr(procinfo_connections[count].src,src)!=NULL )
		&&
		( strstr(procinfo_connections[count].dst,dst)!=NULL )
	    ) {
	    stat = procinfo_connections[count].state;
	    writelog(5,"   --> Connection %s->%s status is %i (%s)",
				src,dst,stat,ax25_state[stat]);
	}
	count++;
    }
    if (stat<0) writelog(1,"Warning: found no connection from %s to %s.",src,dst);

    return stat;

}

