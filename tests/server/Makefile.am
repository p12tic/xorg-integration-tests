include $(top_srcdir)/tests/common/Makefile.am

noinst_PROGRAMS = grab xi2 xigrabbutton xtest multihead misc
TESTS=$(noinst_PROGRAMS)

grab_SOURCES = grab.cpp $(common_sources)
grab_LDADD = $(GTEST_LDADDS)

xi2_SOURCES = xi2.cpp $(common_sources)
xi2_LDADD = $(GTEST_LDADDS)

xigrabbutton_SOURCES = xigrabbutton.cpp $(common_sources)
xigrabbutton_LDADD = $(GTEST_LDADDS)

xtest_SOURCES = xtest.cpp $(common_sources)
xtest_LDADD = $(GTEST_LDADDS) $(XTEST_LIBS)

multihead_SOURCES = multihead.cpp $(common_sources)
multihead_LDADD = $(GTEST_LDADDS)

misc_SOURCES = misc.cpp $(common_sources)
misc_LDADD = $(GTEST_LDADDS)
