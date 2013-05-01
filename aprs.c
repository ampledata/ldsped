/*
 *  aprs.c - agwpe linux replacement
 *
 *  Copyright (C) 2009, Lieven De Samblanx, ON7LDS. All rights reserved
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
#include <sys/wait.h>

#include "ldsped.h"
#include "utils.h"
#include "aprs.h"
#include "agw.h"



int search_command(char *Q, char *command) {
    int i;

    if (scriptpaths==0) return 1;

    for (i=0;i<scriptpaths;i++) {
        if (strncmp(scripts[i].name ,Q,10)==0) {
            writelog(5,"Found ! (%s -> %s)",Q,scripts[i].path);
            strcpy(command,scripts[i].path);
            return 0;
        }
    }
    return 1;
}


void check_ack(char *me, char *from, char *parm){
    char *p;
    AGWFrame frame;
    char s1[20], s2[100], s3[30];

    frame.Port=0;
    strcpy(frame.CallFrom,me);
    strcpy(frame.CallTo,APDEST);

    frame.DataKind=76;
    frame.PID=0;
    frame.DataLen=0;

    writelog(6,"Checking if ACK wanted (%s)...",parm);
    p=strstr(parm,"{");
    if (p != NULL) {
        p[0]='\0';
        sprintf(s2,":%s",from);
        while ( strlen(s2)<10 ) strcat(s2," ");
        strcat(s2,":");

        strcpy(s1,frame.CallTo);
        sprintf(s3,"ack%s",p+1);
        strcat(s2,s3);
        writelog(3,"ONAIR: ACK-ing message %s to %s",p+1,from);
        sendbeacon(frame, s2, strlen(s2), s1);

    } else {
        writelog(6,"No ACK needed");
    }
}


void check_childs() {
    int i,j,status,pid;

    for (i=0;i<scriptchilds;i++) {
        pid=waitpid(scriptchild[i].PID,&status,WNOHANG);
        if (pid>0) {
            scriptchilds--;
            writelog(4,"Script terminated: pid %d status %d (%d left)",pid,status,scriptchilds);
            for (j=i;j<scriptchilds;j++) {
                scriptchild[i].PID=scriptchild[j].PID;
                strcpy(scriptchild[i].name,scriptchild[j].name);
                }
            } else {
                if ( (scriptchild[i].starttime+ MAX_CHILD_EXEX_TIME) < time(NULL) ) {
                    writelog(2,"Child runtime exceeded, killing child with PID %d",scriptchild[i].PID);
                    kill( scriptchild[i].PID, SIGKILL );
                }
            }
    }
}



void APRSS(char *me,char *from, char *parm)
{
    AGWFrame frame;
    char s1[100], s2[100];

    frame.Port=0;
    strcpy(frame.CallFrom,me);
    strcpy(frame.CallTo,APDEST);

    frame.DataKind=76;
    frame.PID=0;
    frame.DataLen=0;

    sprintf(s2,":%s",from);
    while ( strlen(s2)<10 ) strcat(s2," ");
    strcat(s2,":");
    strcat(s2,station_text);

    strcpy(s1,frame.CallTo);

    writelog(3,"ONAIR: APRSS response to %s",from);
    sendbeacon(frame, s2, strlen(s2), s1);
}



void APRSP(char *me,char *from, char *parm)
{
    AGWFrame frame;
    char s1[100], s2[100];

    frame.Port=0;
    strcpy(frame.CallFrom,me);
    strcpy(frame.CallTo,APDEST);

    frame.DataKind=76;
    frame.PID=0;
    frame.DataLen=0;

    sprintf(s2,"=%s {ldsped V%s}",trafficpos,LDSPED_VERSION);

    strcpy(s1,frame.CallTo);

    writelog(3,"ONAIR: APRSP response (%s) to %s",s2,from);
    sendbeacon(frame, s2, strlen(s2), s1);
}


void APRST(char *me,char *from, char *parm)
{
    AGWFrame frame;
    char s1[100], s2[100], *p;
    int i;

    frame.Port=0;
    strcpy(frame.CallFrom,me);
    strcpy(frame.CallTo,APDEST);

    frame.DataKind=76;
    frame.PID=0;
    frame.DataLen=0;

    strncpy(s2,strstr(lprintbuf,"Fm ")+3,10);
    p=strstr(s2," "); p[0]='>'; p[1]=0;
    strncat(s2,strstr(lprintbuf,"To ")+3,10);
    p=strstr(s2," "); p[0]=0;

    if ( strstr(lprintbuf,"Via ") != NULL ) {
	p[0]=','; p[1]=0;
	p=strstr(lprintbuf,"Via ")+4;
	strncpy(s1,p,99); s1[99]=0;
        p=strstr(s1," <");
	if (p!=NULL) { p[0]=0; }

	while ( (p=strstr(s1," "))!= NULL ) {
	    p[0]=',';
	}
	while ( (p=strstr(s1,"*"))!= NULL ) {
	    for (i=0;i<strlen(p);i++) p[i]=p[i+1];
	}
	strcat(s2,s1);
    }


    sprintf(s1,":%s",from);
    while ( strlen(s1)<10 ) strcat(s1," ");
    strcat(s1,":");
    strcat(s1,s2);

    strcpy(s2,frame.CallTo);

    writelog(3,"ONAIR: APRST response (%s) to %s",s1,from);
    sendbeacon(frame, s1, strlen(s1), s2);

}

void APRSQ(char *me,char *from, char *parm)
{
    AGWFrame frame;
    char s1[20], s2[100], s3[300], s4[200];
    pid_t pid;
    FILE *fp;
    int i,j,stat;
    char path[50];
    char comm[68];
    char *l;

    check_childs();

    frame.Port=0;
    strcpy(frame.CallFrom,me);
    strcpy(frame.CallTo,APDEST);

    frame.DataKind=76;
    frame.PID=0;
    frame.DataLen=0;

    sprintf(s2,":%s",from);
    while ( strlen(s2)<10 ) strcat(s2," ");
    strcat(s2,":");

    strcpy(s1,frame.CallTo);
    strcpy(s3,"");

    writelog(5,"Searching command for query '%s'",parm);

    if ( search_command(parm,s3)!=0) {
        writelog(1,"No data found for query");
        return;
    }

    writelog(5,"Data found for query, trying to execute script %d '%s'.",scriptchilds,s3);

    if ( scriptchilds >= MAX_SCRIPT_CHILDS) {
        writelog(0,"ERROR : script %s could not be executed, already %d scripts hanging",s3,scriptchilds);
        return;
    }

    writelog(5,"Starting script %d (max %d)",scriptchilds+1,MAX_SCRIPT_CHILDS);
    pid = fork();
    if (pid == -1) perror("fork");
    if (pid == 0) {
        // This is the child
        setuid(scriptuser);
        setgid(scriptgroup);


        strcpy(comm,path); l=strstr(comm," ");
        if (l!=NULL) { l[0]='\0'; }
        sprintf(s4,"if [ -x %s ]; then %s; else exit 127; fi", comm,s3);

        fp = popen(s4, "r"); s3[0]='\0';
        if (fp == NULL) {
            writelog(0,"Child: APRSQ: Failed to run script (%s)",s3 );
        } else {

            while (fgets(path, sizeof(path)-1, fp) != NULL) {
                path[strlen(path)]='\0';
                strcat(s3,path);
                if ( strlen(s3) > (sizeof(s3)-sizeof(path)+1) )
                    {
                        for (i=sizeof(path);i<=strlen(s3);i++)
                            s3[i-sizeof(path)] = s3[i];
                    }
            }
            i=pclose(fp);
            if (s3[strlen(s3)-1]=='\n') s3[strlen(s3)-1]=0;
            j=strlen(s3)-68;
            if (j>0) {
                for (i = 0; i<68; i++) {
                    if (s3[i+j] == '\n' || s3[i+j] == '\r' || s3[i+j] == '\t') s3[i]=' '; else s3[i]=s3[i+j];
                }
            s3[i]=0;
            }
            if (WIFEXITED(i)) {
                stat=WEXITSTATUS(i);
                if (stat) {
                    writelog(1,"Child: Script terminated with exit code %d", stat);
                    s3[0]='\0';
                }
            }
            if (strlen(s3)>0) {
                strcat(s2,s3);
                writelog(3,"ONAIR: APRSQ response (%s) to %s",s3,from);
//                strcat(s2,"\n");
                sendbeacon(frame, s2, strlen(s2), s1);
            } else {
                writelog(5,"Child: Script failed or returned no data. Not going ONAIR.");
            }
        }
        exit(0);
    } else {
        // This is the parent
//        wait(&stat);
//        printf("PARENT: Child's exit code is: %d\n", WEXITSTATUS(stat));
        strcpy(scriptchild[scriptchilds].name,parm);
        scriptchild[scriptchilds].PID=pid;
        scriptchild[scriptchilds].starttime=time(NULL);
        scriptchilds++;
        writelog(2,"Script %d started with PID %d",scriptchilds,pid);
    }
}



void check_APRS_message(char *buffer, char *message)
{
    char mess[100], from[15], call[10], cmd[6], parm[15], *msg;
    int i;

    from[0]=0; call[0]=0; cmd[0]=0; parm[0]=0;

    msg=strstr(lprintbuf,"Fm ");
    if (msg != NULL) {
	strncpy(from,msg+3,15);
	msg=strstr(from,"To ");
	if (msg != NULL) { msg--; msg[0]=0; }
    }

    strncpy(mess,message,85); msg=&mess[1];

    // data type 'message'
    // :<addressee (9 bytes)>:<message text (0-67 bytes)>
    // // writelog(0,"--------------<%s>-----<%s>",msg,from);
    if (strlen(msg)>16 ) {
        strncpy(call,++msg,9); call[9]=32;
        for (i=9;i>0;i--) { if (call[i]==32) { call[i]=0; }; }
        msg+=9;
        if ( (msg[0]==':') && (msg[1]=='?') ){
	    msg++;
	    strncpy(cmd,++msg,5); cmd[5]=0;
	    msg+=5;
	    if (msg[0]==' ') msg++;
	    strncpy(parm,msg,15);  parm[14]=0;
	    for (i=0;i<15;i++) { if (parm[i]=='\n') {parm[i]='\0';} }
	    writelog(4,"APRS Query from %s for %s: query=%s, parameter='%s'",from,call,cmd,parm);
	
//            usleep(500000);

	    for (i=0;i<ports;i++)
		{
		    if ( strcmp(portlist[i].call,call)==0 ) {
			check_ack(call,from,parm);

			i=ports;
			writelog(2,"APRS Query from %s for me (%s): query=%s, parameter='%s'",from,call,cmd,parm);
			if ( strcmp(cmd,"APRSS")==0 ) APRSS(call,from,parm);
			if ( strcmp(cmd,"APRSP")==0 ) APRSP(call,from,parm);
			if ( strcmp(cmd,"APRST")==0 ) APRST(call,from,parm);
			if ( strcmp(cmd,"PING?")==0 ) APRST(call,from,parm);
			if ( strcmp(cmd,"APRSQ")==0 ) APRSQ(call,from,parm);
		    }
		}
	}
    }
}
