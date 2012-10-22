#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>

#include <xorg/gtest/xorg-gtest.h>

#include "input-driver-test.h"
#include "device-interface.h"
#include "helpers.h"

class EventQueueTest : public InputDriverTest,
                       public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        InputDriverTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    virtual void SetUpConfigAndLog(const std::string &param) {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "kbd-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig("/tmp/eventqueue-test.conf");
        server.SetOption("-logfile", "/tmp/Xorg-eventqueue-test.log");
        server.SetOption("-config", config.GetPath());
    }
};

TEST_F(EventQueueTest, mieqOverflow)
{
    SCOPED_TRACE("\n"
                 "TESTCASE: overflow the event queue and force a resize.\n"
                 "Search the server log for the error message.\n");

    for (int i = 0; i < 2048; i++)
        dev->PlayOne(EV_REL, REL_X, -1, true);
    XSync(Display(), False);
    ASSERT_EQ(server.GetState(), xorg::testing::Process::RUNNING);

    std::ifstream logfile(server.GetLogFilePath().c_str());
    std::string line;
    std::string bug_warn("EQ overflowing");
    bool found = false;
    ASSERT_TRUE(logfile.is_open());
    while(!found && getline(logfile, line))
        found = line.find(bug_warn);

    ASSERT_TRUE(found) << "Expected message '" << bug_warn << "' in the log "
                       << "but couldn't find it";
}


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
