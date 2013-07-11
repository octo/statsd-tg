AC_PREREQ([2.68])
AC_INIT([statsd-tg], [1.0.0], [ff@octo.it])
AC_CONFIG_SRCDIR([src/statsd-tg.c])
AC_CONFIG_HEADERS([src/config.h])

AM_INIT_AUTOMAKE([tar-pax dist-bzip2])

AC_DEFINE([_ISOC99_SOURCE], [], [Request C99 mode])
AC_DEFINE([_POSIX_C_SOURCE], [200112L], [Request POSIX 2001])
AC_DEFINE([_XOPEN_SOURCE], [500], [Request XOpen issue 5])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([netdb.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([memset socket strerror strtol])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
