/*
*  latlon.c - agwpe linux replacement
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



/*
    Sorry, this is not very beautiful code

    I try to spend time coding ldped, not coding this ...
    This only is a supporting program for the configuration script.
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT "80"
#define SERVER "aprs.fi"

#define MAXDATASIZE 1000
#define MAXPAGINASIZE 10000

//#define DEBUG 1


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int sockfd, numbytes, i,j, d1,d2,d3,d4,d5,d6;
    char server[20], buf[MAXDATASIZE], blz[10000], position[20],  call[20], *antw, *antw2, e1,e2;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if ( (argc != 2) || ( strlen(argv[1])<4 ) || ( strlen(argv[1])>8 ) ) {
        fprintf(stderr,"Error: missing call\n");
        exit(1);
    }

    strcpy(server,SERVER);
    sprintf(call,"/info/%s",argv[1]);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(server, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

	/* getaddrinfo() returns a list of address structures. 
	    Try each address until we successfully bind.
	    If socket fails, we (close the socket and) try the next address. */

        for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);

    // OK, got the server. Let's connect.
#ifdef DEBUG
     printf("client: connecting to %s\n", s);
#endif

    //Find info for Call : request page
    sprintf(buf,"GET %s HTTP/1.0\nHost: aprs.fi\n\n ",call);

    //read data
    if ((numbytes = send(sockfd, buf, strlen(buf), 0)) == -1) {
        perror("send");
        exit(1);
    }

    blz[0] = '\0';
    while ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) >0) {

        buf[numbytes] = '\0';
	if ((strlen(blz)+numbytes) < 9999) { strncat(blz,buf,numbytes); }
    }

    //Search result : was it OK or not ?
    antw=strstr(blz,"HTTP/1");
    if (antw == NULL) { fprintf(stderr,"Unknown response\n"); exit(1); }
    antw=strstr(blz,"\n"); antw--; antw[0]=0;

#ifdef DEBUG
    printf("client: received (%s)\n",blz);
#endif
    if (strstr(blz,"404") != NULL) {
	printf("Target not found\n");
	exit(1);
    }


    if (strstr(blz,"301") != NULL) {
	// Got redirect answer. OK, load new page
	close(sockfd);
        // loop through all the results and connect to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
                    p->ai_protocol)) == -1) {
                perror("client: socket");
                continue;
            }

            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                perror("client: connect");
                continue;
            }

            break;
        }

#ifdef DEBUG
	printf("redirect to ");
#endif
	antw[0]=' ';
	antw=strstr(blz,"Location: "); antw+=10;
	antw2=strstr(antw,"\n"); 
	if (antw2==NULL) { fprintf(stderr,"Unknown redirect response\n"); exit(1); }
	antw2--; antw2[0]=0;
	strncpy(call,antw,15);
        sprintf(buf,"GET %s HTTP/1.0\nHost: aprs.fi\n\n ",antw);
#ifdef DEBUG
	printf("%s\n",antw);
#endif
        if ((numbytes = send(sockfd, buf, strlen(buf), 0)) == -1) {
	    perror("send");
	    exit(1);
        }

	blz[0] = '\0';
	while ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) >0) {
	    buf[numbytes] = '\0';
	    if ((strlen(blz)+numbytes) < 9999) { strncat(blz,buf,numbytes); }
	}

    }

    if (strstr(blz,"200") != NULL) {
	// Got good page, so load page
	antw[0]=' ';
#ifdef DEBUG
	printf("Page found\n");
#endif
	antw=strstr(blz," - locator ");
	if (antw==NULL) { fprintf(stderr,"Position not found\n"); exit(1); }
	antw[0]=0; 
	for (i=0;i<30;i++) { if (antw[0] == '>') {break;} else {antw--;} };
	antw++;
    }
#ifdef DEBUG
    printf("client: received (%s)\n",antw);
#endif
    close(sockfd);
    freeaddrinfo(servinfo); // all done with this structure


    // Found position
    i=0;

    position[0]='\0'; j=0;
    while ( (antw[i]>='0') && (antw[i]<='9') && (i<30) ) { position[j++]=antw[i]; position[j]='\0'; i++; };
    while ( ! ((antw[i]>='0') && (antw[i]<='9')) && (i<30) ) { position[j++]=antw[i]; position[j]='\0'; i++; };
    d1=atoi(position);

    position[0]='\0'; j=0;
    while ( (antw[i]>='0') && (antw[i]<='9') && (i<30) ) { position[j++]=antw[i]; position[j]='\0'; i++; };
    while ( ! ((antw[i]>='0') && (antw[i]<='9')) && (i<30) ) { position[j++]=antw[i]; position[j]='\0'; i++; };
    d2=atoi(position);

    position[0]='\0'; j=0;
    while ( (antw[i]>='0') && (antw[i]<='9') && (i<30) ) { position[j++]=antw[i]; position[j]='\0'; i++; };
    while ( ((antw[i]!='N') && (antw[i]!='S')) && (i<30) ) { i++; };
    d3=atoi(position)*100; d3=d3/60;
    e1=antw[i];
    while ( ! ((antw[i]>='0') && (antw[i]<='9')) && (i<30) ) { position[j++]=antw[i]; position[j]='\0'; i++; };



    position[0]='\0'; j=0;
    while ( (antw[i]>='0') && (antw[i]<='9') && (i<30) ) { position[j++]=antw[i]; position[j]='\0'; i++; };
    while ( ! ((antw[i]>='0') && (antw[i]<='9')) && (i<30) ) { position[j++]=antw[i]; position[j]='\0'; i++; };
    d4=atoi(position);

    position[0]='\0'; j=0;
    while ( (antw[i]>='0') && (antw[i]<='9') && (i<30) ) { position[j++]=antw[i]; position[j]='\0'; i++; };
    while ( ! ((antw[i]>='0') && (antw[i]<='9')) && (i<30) ) { position[j++]=antw[i]; position[j]='\0'; i++; };
    d5=atoi(position);

    position[0]='\0'; j=0;
    while ( (antw[i]>='0') && (antw[i]<='9') && (i<30) ) { position[j++]=antw[i]; position[j]='\0'; i++; };
    while ( ((antw[i]!='E') && (antw[i]!='W')) && (i<30) ) { i++; };
    d6=atoi(position)*100; d6=d6/60;
    e2=antw[i];
    while ( ! ((antw[i]>='0') && (antw[i]<='9')) && (i<30) ) { position[j++]=antw[i]; position[j]='\0'; i++; };


    antw=&call[6];
    printf("OK\n%s\n%02d%02d.%02d%c\n%03d%02d.%02d%c\n",antw,d1,d2,d3,e1,d4,d5,d6,e2);
    return 0;
}

