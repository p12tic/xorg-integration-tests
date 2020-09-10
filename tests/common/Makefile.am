include $(top_srcdir)/common.mk

noinst_LIBRARIES = $(XORG_GTEST_BUILD_LIBS) libxit.a

common_sources =  \
		 $(top_srcdir)/tests/common/xit-event.h \
		 $(top_srcdir)/tests/common/xit-property.h \
		 $(top_srcdir)/tests/common/xit-property.cpp \
		 $(top_srcdir)/tests/common/xit-server-test.cpp \
		 $(top_srcdir)/tests/common/xit-server-test.h \
		 $(top_srcdir)/tests/common/xit-server.cpp \
		 $(top_srcdir)/tests/common/xit-server.h \
		 $(top_srcdir)/tests/common/xit-server-input-test.cpp \
		 $(top_srcdir)/tests/common/xit-server-input-test.h \
		 $(top_srcdir)/tests/common/xorg-conf.cpp \
		 $(top_srcdir)/tests/common/xorg-conf.h \
		 $(top_srcdir)/tests/common/helpers.cpp \
		 $(top_srcdir)/tests/common/helpers.h \
		 $(top_srcdir)/tests/common/device-interface.cpp \
		 $(top_srcdir)/tests/common/device-interface.h  \
		 $(top_srcdir)/tests/common/device-emulated-interface.cpp \
		 $(top_srcdir)/tests/common/device-emulated-interface.h  \
		 $(top_srcdir)/tests/common/video-driver-test.h

libxit_a_SOURCES = $(common_sources)

# Here and below we compile without warnings (-w) because the projects using
# xorg-gtest will not want to see warnings or fail to build due to warnings in
# gtest or xorg-gtest.
nodist_libgtest_a_SOURCES = $(XORG_GTEST_SOURCE)/src/gtest/gtest-all.cc
libgtest_a_CPPFLAGS = $(GTEST_CPPFLAGS) $(AM_CPPFLAGS) -w
libgtest_a_CXXFLAGS = $(GTEST_CXXFLAGS) $(AM_CXXFLAGS)

nodist_libgtest_main_a_SOURCES = $(XORG_GTEST_SOURCE)/src/gtest/gtest_main.cc
libgtest_main_a_CPPFLAGS = $(GTEST_CPPFLAGS) $(AM_CPPFLAGS) -w
libgtest_main_a_CXXFLAGS = $(GTEST_CXXFLAGS) $(AM_CXXFLAGS)

nodist_libxorg_gtest_a_SOURCES = $(XORG_GTEST_SOURCE)/src/xorg-gtest-all.cpp
libxorg_gtest_a_CPPFLAGS = \
	$(XORG_GTEST_CPPFLAGS) \
	$(GTEST_CPPFLAGS) \
	$(AM_CPPFLAGS) \
	-iquote$(XORG_GTEST_SOURCE) \
	-w
libxorg_gtest_a_CXXFLAGS = \
	$(XORG_GTEST_CXXFLAGS) \
	$(GTEST_CXXFLAGS) \
	$(AM_CPPFLAGS)

nodist_libxorg_gtest_main_a_SOURCES = \
	$(XORG_GTEST_SOURCE)/src/xorg-gtest_main.cpp
libxorg_gtest_main_a_CPPFLAGS = \
	$(XORG_GTEST_CPPFLAGS) \
	$(GTEST_CPPFLAGS) \
	$(AM_CPPFLAGS) \
	-iquote$(XORG_GTEST_SOURCE) \
	-w
libxorg_gtest_main_a_CXXFLAGS = \
	$(XORG_GTEST_CXXFLAGS) \
	$(GTEST_CXXFLAGS) \
	$(AM_CXXFLAGS)

