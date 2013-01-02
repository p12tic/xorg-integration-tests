include $(top_srcdir)/tests/common/Makefile.am

noinst_PROGRAMS = legacy-drivers \
                  input-module-load \
                  kbd-driver \
                  mouse-driver \
                  evdev-driver \
                  synaptics-driver \
                  wacom-driver

TESTS=$(noinst_PROGRAMS)

legacy_drivers_SOURCES = legacy-drivers.cpp
legacy_drivers_LDADD = $(GTEST_LDADDS) libxit.a

input_module_load_SOURCES = input-module-load.cpp
input_module_load_LDADD = $(GTEST_LDADDS) libxit.a

kbd_driver_SOURCES = kbd.cpp
kbd_driver_LDADD = $(GTEST_LDADDS) libxit.a

mouse_driver_SOURCES = mouse.cpp
mouse_driver_LDADD = $(GTEST_LDADDS) libxit.a

evdev_driver_SOURCES = evdev.cpp
evdev_driver_LDADD = $(GTEST_LDADDS) libxit.a

synaptics_driver_SOURCES = synaptics.cpp
synaptics_driver_LDADD = $(GTEST_LDADDS) libxit.a

wacom_driver_SOURCES = wacom_devs.h				\
		       wacom-hovering-expresskeys.cpp		\
		       wacom-input-matrix.cpp			\
		       wacom.cpp
wacom_driver_LDADD = $(GTEST_LDADDS) libxit.a

