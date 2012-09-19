#  Copyright 2012 Red Hat, Inc.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Process this file with autoconf to produce a configure script

# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([xorg-integration-tests], [0.0.1])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR(.)

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2 no-dist-gzip tar-pax])
AM_MAINTAINER_MODE

# Initialize libtool
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CXX

m4_ifndef([XORG_MACROS_VERSION], 
          [m4_fatal([must install xorg-macros 1.17 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.17)
XORG_DEFAULT_OPTIONS
XORG_ENABLE_INTEGRATION_TESTS(yes)
XORG_WITH_DOXYGEN

PKG_CHECK_MODULES(XI, x11 xi)
PKG_CHECK_MODULES(EVEMU, [evemu >= 1.0.10])
PKG_CHECK_MODULES(XI22, [inputproto >= 2.2] [xi >= 1.6],
                  AC_DEFINE(HAVE_XI22, 1, [XI 2.2 available]),
                  [noop=noop])

PKG_CHECK_MODULES(XORGGTEST, [xorg-gtest >= 0.4.0])

m4_ifndef([CHECK_XORG_GTEST],
          [m4_fatal([must install xorg-gtest before running autoconf/autogen])])
CHECK_XORG_GTEST([], AC_MSG_ERROR([xorg-gtest or its dependencies not found.  Did you specify --with-gtest-source-path?]))

# Check for RHEL and define as appropriate
if test -f /etc/redhat-release; then
    [rhel_version=`grep -o "[5-7].[0-9]\+" /etc/redhat-release`]
    case "$rhel_version" in
        5.*)
        AC_DEFINE(HAVE_RHEL5, [1], [Building on RHEL5])
        ;;
        6.*)
        AC_DEFINE(HAVE_RHEL6, [1], [Building on RHEL6])
        ;;
        7.*)
        AC_DEFINE(HAVE_RHEL7, [1], [Building on RHEL7])
        ;;
        **)
        ;;
    esac
    AC_DEFINE_UNQUOTED(RHEL_VERSION, [$rhel_version], "RHEL version")
fi
AM_CONDITIONAL(HAVE_RHEL, [test "x$rhel_version" != "x"])

AC_CONFIG_FILES([Makefile
                 doc/Doxyfile
                 doc/Makefile
                 recordings/Makefile
                 tests/Makefile
                 tests/common/Makefile
                 tests/input/Makefile
                 tests/video/Makefile
                 tests/server/Makefile
                 tests/lib/Makefile])
AC_OUTPUT
