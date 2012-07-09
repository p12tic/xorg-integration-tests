noinst_PROGRAMS = acecad test-input-load

include $(top_srcdir)/Makefile-xorg-gtest.am
noinst_LIBRARIES = $(XORG_GTEST_BUILD_LIBS)

TESTS=$(noinst_PROGRAMS)

AM_CPPFLAGS = \
	      $(XI_CFLAGS) \
	      $(GTEST_CPPFLAGS) \
	      $(XORG_GTEST_CPPFLAGS) \
	      -DDEFAULT_XORG_SERVER=\"$(prefix)/bin/Xorg\" \
	      -DTEST_ROOT_DIR=\"$(abs_top_srcdir)/tests/\"

AM_CXXFLAGS = $(GTEST_CXXFLAGS) $(XORG_GTEST_CXXFLAGS)

GTEST_LDADDS = \
	       $(XI_LIBS) \
	       $(XORG_GTEST_LIBS) \
	       $(EVEMU_LIBS) \
	       $(XORG_GTEST_MAIN_LIBS)

acecad_SOURCES = acecad.cpp
acecad_LDADD = $(GTEST_LDADDS)

test_input_load_SOURCES = test-input-load.cpp
test_input_load_LDADD = $(GTEST_LDADDS)
