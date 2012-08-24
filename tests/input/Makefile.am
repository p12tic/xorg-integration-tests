include $(top_srcdir)/tests/common/Makefile.am

noinst_PROGRAMS = legacy-drivers \
                  test-input-load \
                  kbd-driver \
                  mouse-driver \
                  evdev-driver \
                  synaptics-driver \
                  wacom-driver

TESTS=$(noinst_PROGRAMS)

legacy_drivers_SOURCES = legacy-drivers.cpp $(common_sources)
legacy_drivers_LDADD = $(GTEST_LDADDS)

test_input_load_SOURCES = test-input-load.cpp $(common_sources)
test_input_load_LDADD = $(GTEST_LDADDS)

kbd_driver_SOURCES = kbd.cpp $(common_sources)
kbd_driver_LDADD = $(GTEST_LDADDS)

mouse_driver_SOURCES = mouse.cpp $(common_sources)
mouse_driver_LDADD = $(GTEST_LDADDS)

evdev_driver_SOURCES = evdev.cpp $(common_sources)
evdev_driver_LDADD = $(GTEST_LDADDS)

synaptics_driver_SOURCES = synaptics.cpp $(common_sources)
synaptics_driver_LDADD = $(GTEST_LDADDS)

wacom_driver_SOURCES = wacom.cpp $(common_sources)
wacom_driver_LDADD = $(GTEST_LDADDS)
