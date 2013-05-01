/*
    adapted from mheard.c
    by Lieven De Samblanx, ON7LDS.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <config.h>

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
#include <netax25/mheard.h>


#include <ldsped.h>
#include <utils.h>
#include <agw.h>


struct PortRecord {
	struct mheard_struct entry;
	struct PortRecord *Next;
};


static struct PortRecord *PortList = NULL;


static void PrintPortEntry(struct PortRecord *pr, int portnr, int c_index)
{
	char lh[30], fh[30], *call, *s, data[255];
	int i;
	
	strcpy(lh, ctime(&pr->entry.last_heard));
	lh[19] = 0;
	strcpy(fh, ctime(&pr->entry.first_heard));
	fh[19] = 0;
	call = ax25_ntoa(&pr->entry.from_call);
	if ((s = strstr(call, "-0")) != NULL)
		*s = '\0';
	sprintf(data,"%s %s %s ", call, fh, lh);
	for (i=strlen(data);i<sizeof(data);i++)
	    data[i]=0;
	send_frame('H',connection[c_index].fd,portlist[portnr].dev,data,strlen(data)+33,"","");
	
}

static void ListOnlyPort(int port, int c_index)
{
	struct PortRecord *pr;
	struct PortRecord *pr2;
	int i=0;

	writelog(4, "   --> Sending mheard data for port {%s}",portlist[port].name);
	for (pr = PortList; (pr != NULL) && (i<20) ; ) {
		pr2 = pr;
		if (strcmp(pr->entry.portname, portlist[port].name) == 0)
			{ PrintPortEntry(pr,port,c_index); i++; }
		pr = pr->Next;
		free(pr2);
		}
	PortList=NULL;
	writelog(5, "   --> Sent %i lines heard data",i);
}

static void LoadPortData(void)
{
	FILE *fp;
	struct PortRecord *pr;
	struct mheard_struct mheard;

	if ((fp = fopen(DATA_MHEARD_FILE, "r")) == NULL) {
		writelog(0, "ERROR: mheard: cannot open mheard data file");
		exit(1);
	} else {
	    writelog(6,"========> Open fd ?? in LoadPortData");
	}
	

	while (fread(&mheard, sizeof(struct mheard_struct), 1, fp) == 1) {
		pr = malloc(sizeof(struct PortRecord));
		pr->entry = mheard;
		pr->Next  = PortList;
		PortList  = pr;
	}

	if (fclose(fp)<0)
	    writelog(0, "ERROR: lp/closing file, %s",strerror(errno));
}

static void SortByTime(void)
{
	struct PortRecord *p = PortList;
	struct PortRecord *n;
	PortList = NULL;

	while (p != NULL) {
		struct PortRecord *w = PortList;

		n = p->Next;

		if (w == NULL || p->entry.last_heard > w->entry.last_heard) {
			p->Next  = w;
			PortList = p;
			p = n;
			continue;
		}

		while (w->Next != NULL && p->entry.last_heard <= w->Next->entry.last_heard)
			w = w->Next;

		p->Next = w->Next;
		w->Next = p;
		p = n;
	}
}


int checkmhearddata(void)
{
	FILE *fp;

	if ((fp = fopen(DATA_MHEARD_FILE, "r")) == NULL) {
		writelog(1, "Cannot open mheard data file (%s),",DATA_MHEARD_FILE);
		writelog(1, " so I will not process H frames.");
		return 0;
	} else {
	    writelog(6,"========> Open fd ?? in checkmhearddata");
	    if (fclose(fp)<0)
		writelog(0, "ERROR: cm/closing file, %s",strerror(errno));
	}
    return 1;
}


void mheard(int port, int c_index)
{
    LoadPortData();
    SortByTime(); 
    ListOnlyPort(port,c_index);
}
