include $(top_srcdir)/tests/common/Makefile.am

noinst_PROGRAMS = grab touch xtest multihead misc barriers
TESTS=$(noinst_PROGRAMS)

grab_SOURCES = grab.cpp xigrabbutton.cpp $(common_sources)
grab_LDADD = $(GTEST_LDADDS)

touch_SOURCES = touch.cpp $(common_sources)
touch_LDADD = $(GTEST_LDADDS)

xtest_SOURCES = xtest.cpp $(common_sources)
xtest_LDADD = $(GTEST_LDADDS) $(XTEST_LIBS)

multihead_SOURCES = multihead.cpp $(common_sources)
multihead_LDADD = $(GTEST_LDADDS)

misc_SOURCES = misc.cpp $(common_sources)
misc_LDADD = $(GTEST_LDADDS) $(XSCREENSAVER_LIBS)

barriers_SOURCES = barrires-common.h barriers-validity.cpp barriers-directions.cpp $(common_sources)
barriers_LDADD = $(GTEST_LDADDS) $(XFIXES_LIBS)
