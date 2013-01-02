include $(top_srcdir)/Makefile-xorg-gtest.am

AM_CPPFLAGS = \
	      $(XI_CFLAGS) \
	      $(XFIXES_CFLAGS) \
	      $(XRANDR_CFLAGS) \
	      $(WACOM_CFLAGS) \
	      $(GTEST_CPPFLAGS) \
	      $(XORG_GTEST_CPPFLAGS) \
              -Wall \
	      -I$(top_srcdir)/tests/common \
	      -DTEST_ROOT_DIR=\"$(abs_top_srcdir)/tests/\" \
	      -DRECORDINGS_DIR=\"$(abs_top_srcdir)/recordings/\"

AM_CXXFLAGS = $(GTEST_CXXFLAGS) $(XORG_GTEST_CXXFLAGS)

GTEST_LDADDS = \
	       $(XI_LIBS) \
	       $(XRANDR_LIBS) \
	       $(XORG_GTEST_LIBS) \
	       $(EVEMU_LIBS) \
	       $(XORG_GTEST_MAIN_LIBS)

XIT_LIBS=$(top_builddir)/tests/common/libxit.a
