#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"
#include "helpers.h"

class KeyboardDriverTest : public InputDriverTest {
    virtual void SetUpConfigAndLog(const std::string &prefix) {
        server.SetOption("-logfile", "/tmp/Xorg-kbd-driver.log");
        server.SetOption("-config", "/tmp/kbd-driver.conf");

        config.AddInputSection("kbd", "--device--", "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig("/tmp/kbd-driver.conf");
    }
};


TEST_F(KeyboardDriverTest, DeviceExists)
{
    std::string param;
    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(Display(), XIAllDevices, &ndevices);
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

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
