/*
 *  utils.h - agwpe linux replacement
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

#ifndef UTILS_HEADERS
#define UTILS_HEADERS 1

#include "agw.h"


#ifdef HAVE_NETAX25_AX25_H
#include <netax25/ax25.h>
#else
#include <netax25/kernel_ax25.h>
#endif

#include <netax25/axconfig.h>


/*
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
*/


#define T_ERROR         1
#define T_PORT          2
#define T_KISS          3
#define T_BPQ           4
#define T_DATA          5
#define T_PROTOCOL      6
#define T_AXHDR         7
#define T_ADDR          8
#define T_IPHDR         9
#define T_TCPHDR        10
#define T_ROSEHDR       11
#define T_TIMESTAMP     12
#define T_FLEXNET       13

#define _PATH_PROCNET_AX25  "/proc/net/ax25"

//windows SYSTEMTIME structure for returning in agw frames
typedef struct _SYSTEMTIME {
    unsigned int wYear;
    unsigned int wMonth;
    unsigned int wDayOfWeek;
    unsigned int wDay;
    unsigned int wHour;
    unsigned int wMinute;
    unsigned int wSecond;
    unsigned int wMilliseconds;
}SYSTEMTIME, *PSYSTEMTIME;

int timestamp;
int ports;
int trafficperiod;
int probeperiod;
int packets_mrtg_i,packets_mrtg_o;
char phgr[100], trafficpos[25], traffic_unproto[100], trafficcall[10], trafficname[10],
     probe_unproto[100], probecall[10];

/* In kissdump.c */
void ki_dump(unsigned char *, int, int);

/* ax25dump.c */
void ax25_dump(unsigned char *, int, int);
char *pax25(char *, unsigned char *);

/* mheard.c */
int checkmhearddata(void);
void mheard(int, int);

/* beacon.c */
int sendbeacon(AGWFrame, char *, int, char *);


void clear_stats(void);
void init_station(char *,int);
void init_traffic(char *,int);
void init_probe(char *,int);
void init_phgr(char *);
void init_axprms(char *, int);
void init_script(char *, int);
int check_parameters(void);
void check_probe(void);
int count_packets(int);
void port_byte_count(int ,char *);
void put_traffic_stats(int);
void dump_info(void);
void display_timestamp(void);
void writestring(char *buf, int size, char *wstring, int wsize);
int write_buffer(int fd, char *buf, int count);
void writelog(int, char *fmt, ...);
int subst_char(char *, int, int);
void init_ax_config_calls();
void clear_lprintbuf();
int write_lprintbuf(int conn_nr, char *port);
void lprintf(int dtype, char *fmt, ...);

void data_dump(unsigned char *, int, int);

void netrom_dump(unsigned char *, int, int, int);
void arp_dump(unsigned char *, int);
void ip_dump(unsigned char *, int, int);
void icmp_dump(unsigned char *, int, int);
void udp_dump(unsigned char *, int, int);
void tcp_dump(unsigned char *, int, int);
void rspf_dump(unsigned char *, int);
void rip_dump(unsigned char *, int);
void rose_dump(unsigned char *, int, int);
void flexnet_dump(unsigned char *, int, int);
int ax25_proc_info(void);
int search_outstanding_bytes(char *, char *);
int search_connection_status(char *, char *);

#endif
