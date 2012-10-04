include $(top_srcdir)/Makefile-xorg-gtest.am

noinst_LIBRARIES = $(XORG_GTEST_BUILD_LIBS)

AM_CPPFLAGS = \
	      $(XI_CFLAGS) \
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

common_sources = $(top_srcdir)/tests/common/input-driver-test.cpp \
		 $(top_srcdir)/tests/common/input-driver-test.h \
		 $(top_srcdir)/tests/common/xorg-conf.cpp \
		 $(top_srcdir)/tests/common/xorg-conf.h \
		 $(top_srcdir)/tests/common/helpers.cpp \
		 $(top_srcdir)/tests/common/helpers.h \
		 $(top_srcdir)/tests/common/device-interface.cpp \
		 $(top_srcdir)/tests/common/device-interface.h  \
		 $(top_srcdir)/tests/common/video-driver-test.cpp \
		 $(top_srcdir)/tests/common/video-driver-test.h
