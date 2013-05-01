
etcfiles = ldsped.conf.example

installconf:
	$(mkinstalldirs) $(DESTDIR)$(AX25_SYSCONFDIR)
	@list='$(etcfiles)'; for p in $$list; do \
	  echo " $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p"; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(AX25_SYSCONFDIR)/$$p; \
	done


sbin_PROGRAMS = ldsped latlon 
#logcall


LDADD = $(AX25_LIB)

INCLUDES = -DAX25_SYSCONFDIR=\""$(AX25_SYSCONFDIR)"\" \
	  -DAX25_LOCALSTATEDIR=\""$(AX25_LOCALSTATEDIR)"\" 

AX25_SYSCONFDIR=$(sysconfdir)/ax25/
AX25_LOCALSTATEDIR=$(localstatedir)/ax25/

man_MANS = ldsped.8 ldsped.conf.5

EXTRA_DIST = $(man_MANS) $(etcfiles)

ldsped_SOURCES =  \
                ldsped.c        \
                aprs.c          \
                utils.c         \
                agw.c           \
                kissdump.c      \
                ax25dump.c      \
                beacon.c        \
                mheard.c        \
                ax25connect.c

latlon_SOURCES = \
                latlon.c

logcall_SOURCES = \
                logcall.c       \
                kissdump.c      \
                ax25dump.c


