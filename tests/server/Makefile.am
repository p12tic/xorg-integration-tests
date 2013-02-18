include $(top_srcdir)/common.mk

noinst_PROGRAMS = server xephyr
TESTS=$(noinst_PROGRAMS)

server_SOURCES = \
                 dga.cpp \
                 grab.cpp \
                 xigrabbutton.cpp \
                 touch.cpp \
                 xtest.cpp \
                 multihead.cpp \
                 misc.cpp \
                 input.cpp \
                 barriers-common.h \
                 barriers-validity.cpp \
                 barriers-directions.cpp \
                 barriers-devices.cpp \
                 barriers-notify.cpp

server_LDADD = $(XIT_LIBS) $(GTEST_LDADDS) $(XTEST_LIBS) $(XFIXES_LIBS) $(XSCREENSAVER_LIBS) $(XIT_LIBS) $(DGA_LIBS) $(PIXMAN_LIBS)

server_CPPFLAGS = $(AM_CPPFLAGS) $(PIXMAN_CFLAGS)

xephyr_SOURCES= xephyr.cpp

xephyr_LDADD = $(XIT_LIBS) $(GTEST_LDADDS)
xephyr_CPPFLAGS = $(AM_CPPFLAGS)
