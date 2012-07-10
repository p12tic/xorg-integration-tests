#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"

TEST_P(InputDriverTest, DriverDevice)
{
    std::string param;
    int ndevices;
    XIDeviceInfo *info;

    param = GetParam();
    info = XIQueryDevice(Display(), XIAllDevices, &ndevices);
    /* VCP, VCK, 2 test devices, forced mouse/keyboard */
    ASSERT_EQ(ndevices, 6) << "Drivers required for this test: mouse, "
                               "keyboard, " << param;

    bool found = false;
    while(ndevices--) {
        if (strcmp(info[ndevices].name, "--device--") == 0) {
            ASSERT_EQ(found, false) << "Duplicate device" << std::endl;
            found = true;
        }
    }

    ASSERT_EQ(found, true);

    XIFreeDeviceInfo(info);
}

INSTANTIATE_TEST_CASE_P(, InputDriverTest,
        ::testing::Values("acecad", "aiptek", "elographics",
                          "fpit", "hyperpen",  "mutouch",
                          "penmount"));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
