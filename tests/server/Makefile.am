include $(top_srcdir)/tests/common/Makefile.am

noinst_PROGRAMS = server
TESTS=$(noinst_PROGRAMS)

server_SOURCES = \
                 grab.cpp \
                 xigrabbutton.cpp \
                 touch.cpp \
                 xtest.cpp \
                 multihead.cpp \
                 misc.cpp \
                 barriers-common.h \
                 barriers-validity.cpp \
                 barriers-directions.cpp \
                 $(common_sources)
server_LDADD = $(GTEST_LDADDS) $(XTEST_LIBS) $(XFIXES_LIBS) $(XSCREENSAVER_LIBS)
