dnl Process this file with autoconf to produce a configure script.
AC_INIT([ldsped], [1.16])
AC_CONFIG_SRCDIR([ldsped.c])

dnl For automake
AM_INIT_AUTOMAKE

AM_CONFIG_HEADER(config.h)

AC_PREFIX_DEFAULT(/usr)
test "$sysconfdir" = '${prefix}/etc' && sysconfdir=/etc

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

dnl Checks for libraries.
AC_SUBST(AX25_LIB)
AC_SUBST(AX25IO_LIB)
AC_CHECK_LIB(ax25, ax25_config_load_ports, AX25_LIB="-lax25", AC_MSG_ERROR(Could not find the libax25 libraries; aborting))

dnl Checks for header files.
AC_HEADER_STDC
dnl--AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h arpa/inet.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h unistd.h)

dnl Checks for working glibc 2.1 headers
AC_MSG_CHECKING(for working netax25/ax25.h header file)
AC_EGREP_HEADER(ax25_fwd_struct, netax25/ax25.h,AC_DEFINE(HAVE_NETAX25_AX25_H, 1, [Defined if netax25/ax25.h is working]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))
dnl--AC_MSG_CHECKING(for working netrom/netrom.h header file)
dnl--AC_EGREP_HEADER(unsigned int ndigis, netrom/netrom.h,AC_DEFINE(HAVE_NETROM_NETROM_H, 1, [Defined if netrom/netrom.h is working]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))
dnl--AC_MSG_CHECKING(for working netrose/rose.h header file)
dnl--AC_EGREP_HEADER(rose_facilities_struct, netrose/rose.h,AC_DEFINE(HAVE_NETROSE_ROSE_H, 1, [Defined if netrose/rose.h is working]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_HEADER_TIME
AC_STRUCT_TM

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_SETVBUF_REVERSED
AC_HEADER_SYS_WAIT
AC_HEADER_TIME
AC_PROG_CC
AC_STRUCT_TM
AC_HEADER_STDC
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_CHECK_FUNCS(gethostbyname getpagesize inet_ntoa memset select socket strcasecmp strchr strerror strstr)

dnl Only use -Wall if we have gcc
if test "x$GCC" = "xyes"; then
  if test -z "`echo "$CFLAGS" | grep "\-Wall" 2> /dev/null`" ; then
    CFLAGS="$CFLAGS -Wall"
  fi
fi

AC_OUTPUT(Makefile)
