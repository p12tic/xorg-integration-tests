include $(top_srcdir)/common.mk

noinst_PROGRAMS = libXi libX11
TESTS=$(noinst_PROGRAMS)

libXi_SOURCES = libXi.cpp
libXi_LDADD = $(XIT_LIBS) $(GTEST_LDADDS)

libX11_SOURCES = libX11.cpp
libX11_LDADD = $(XIT_LIBS) $(GTEST_LDADDS)
