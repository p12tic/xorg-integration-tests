# serial 2

# Copyright (C) 2012 Canonical, Ltd.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Checks whether the gtest source is available on the system. Allows for
# adjusting the include and source path. Sets have_gtest=yes if the source is
# present. Sets GTEST_CPPFLAGS and GTEST_SOURCE to the preprocessor flags and
# source location respectively.
AC_DEFUN([_CHECK_GTEST],
[
  AC_ARG_WITH([gtest-source-path],
              [AS_HELP_STRING([--with-gtest-source-path],
                              [location of the Google test sources, defaults to /usr/src/gtest])],
              [GTEST_SOURCE="$withval"; GTEST_CPPFLAGS="-I$withval/include"],
              [GTEST_SOURCE="/usr/src/gtest"])

  AC_ARG_WITH([gtest-include-path],
              [AS_HELP_STRING([--with-gtest-include-path],
                              [location of the Google test headers])],
              [GTEST_CPPFLAGS="-I$withval"])

  GTEST_CPPFLAGS="$GTEST_CPPFLAGS -I$GTEST_SOURCE"

  AC_CHECK_FILES([$GTEST_SOURCE/src/gtest-all.cc]
                 [$GTEST_SOURCE/src/gtest_main.cc],
                 [have_gtest=yes],
                 [have_gtest=no])

  AS_IF([test "x$have_gtest_source" = xyes],
        [AC_SUBST(GTEST_CPPFLAGS)]
        [AC_SUBST(GTEST_SOURCE)])
]) # _CHECK_GTEST

# CHECK_XORG_GTEST([ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
#
# Checks whether the xorg-gtest source is available on the system. Allows for
# adjusting the include and source path. Sets have_xorg_gtest=yes if the source
# is present. Sets XORG_GTEST_CPPFLAGS and XORG_GTEST_SOURCE to the preprocessor
# flags and source location respectively. Sets XORG_GTEST_LIBS to all the
# libraries needed to link against a built xorg-gtest library.
#
# Both default actions are no-ops.
AC_DEFUN([CHECK_XORG_GTEST],
[
  AC_REQUIRE([_CHECK_GTEST])

  PKG_CHECK_EXISTS([xorg-gtest],
                   [have_xorg_gtest=yes],
                   [have_xorg_gtest=no])

  XORG_GTEST_SOURCE=`$PKG_CONFIG --variable=sourcedir --print-errors xorg-gtest`
  XORG_GTEST_CPPFLAGS=`$PKG_CONFIG --variable=CPPflags --print-errors xorg-gtest`
  XORG_GTEST_CPPFLAGS="$GTEST_CPPFLAGS $XORG_GTEST_CPPFLAGS"
  XORG_GTEST_CPPFLAGS="$XORG_GTEST_CPPFLAGS -I$XORG_GTEST_SOURCE"

  PKG_CHECK_MODULES(X11, [x11], [have_x11=yes], [have_x11=no])

  # Check if we should include support for utouch-evemu
  AC_ARG_WITH([evemu],
              [AS_HELP_STRING([--with-evemu],
                              [support Linux input device recording playback
                               (default: enabled if available)])],
              [],
              [with_evemu=check])

  AS_IF([test "x$with_evemu" = xyes],
        [PKG_CHECK_MODULES(EVEMU, [utouch-evemu], [have_xorg_gtest_evemu=yes])],
        [test "x$with_evemu" = xcheck],
        [PKG_CHECK_MODULES(EVEMU,
                           [utouch-evemu],
                           [have_xorg_gtest_evemu=yes],
                           [have_xorg_gtest_evemu=no])])
  AS_IF([test "x$have_xorg_gtest_evemu" = xyes],
        [XORG_GTEST_CPPFLAGS="$XORG_GTEST_CPPFLAGS -DHAVE_EVEMU"])

  AS_IF([test "x$have_gtest" != xyes -o "x$have_x11" != xyes],
        [have_xorg_gtest=no])

  AS_IF([test "x$have_xorg_gtest" = xyes],
        [AC_SUBST(XORG_GTEST_SOURCE)]
        [AC_SUBST(XORG_GTEST_CPPFLAGS)]

        # Get BASE_CXXFLAGS and STRICT_CXXFLAGS
        [XORG_MACROS_VERSION(1.17)]
        [AC_LANG_PUSH([C++])]
        [XORG_STRICT_OPTION]
        [AC_LANG_POP]
        [$1],
        [$2])

]) # CHECK_XORG_GTEST
