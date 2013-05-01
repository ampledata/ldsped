/*
 *  agw.h
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

#ifndef AGW_HEADERS
#define AGW_HEADERS 1 


typedef struct
{
        unsigned char   Port;
        char   res1;
        char   res2;
        char   res3;
        char   DataKind;
        char   res4;
        char   PID;
        char   res5;
        char   CallFrom[10];
        char   CallTo[10];
        unsigned int DataLen;
        unsigned int res6;
} AGWFrame;


int processframe(char *framedata, int size, int conn_index, int monsock);

int send_frame(char, int , char *, char *, int, char *, char *);

int RegisterCall(char *, int);
int UnRegisterCall(char *);
void UnRegisterCalls(int);
int FindRegisteredCall(char *);

#endif
