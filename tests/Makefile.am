noinst_PROGRAMS = legacy-drivers test-input-load kbd-driver evdev-driver

include $(top_srcdir)/Makefile-xorg-gtest.am
noinst_LIBRARIES = $(XORG_GTEST_BUILD_LIBS)

TESTS=$(noinst_PROGRAMS)

AM_CPPFLAGS = \
	      $(XI_CFLAGS) \
	      $(GTEST_CPPFLAGS) \
	      $(XORG_GTEST_CPPFLAGS) \
	      -DTEST_ROOT_DIR=\"$(abs_top_srcdir)/tests/\" \
	      -DRECORDINGS_DIR=\"$(abs_top_srcdir)/recordings/\"

AM_CXXFLAGS = $(GTEST_CXXFLAGS) $(XORG_GTEST_CXXFLAGS)

GTEST_LDADDS = \
	       $(XI_LIBS) \
	       $(XORG_GTEST_LIBS) \
	       $(EVEMU_LIBS) \
	       $(XORG_GTEST_MAIN_LIBS)

common_sources = input-driver-test.cpp input-driver-test.h \
		 xorg-conf.cpp xorg-conf.h \
		 helpers.cpp helpers.h

legacy_drivers_SOURCES = legacy-drivers.cpp $(common_sources)
legacy_drivers_LDADD = $(GTEST_LDADDS)

test_input_load_SOURCES = test-input-load.cpp $(common_sources)
test_input_load_LDADD = $(GTEST_LDADDS)

kbd_driver_SOURCES = kbd.cpp $(common_sources)
kbd_driver_LDADD = $(GTEST_LDADDS)

evdev_driver_SOURCES = evdev.cpp $(common_sources)
evdev_driver_LDADD = $(GTEST_LDADDS)
