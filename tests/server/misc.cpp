#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>

#include <xorg/gtest/xorg-gtest.h>

#include "input-driver-test.h"
#include "device-interface.h"
#include "helpers.h"

using namespace xorg::testing;

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

TEST(MiscServerTest, DoubleSegfault)
{
    SCOPED_TRACE("\n"
                 "TESTCASE: SIGSEGV the server. The server must catch the "
                 "signal, clean up and then call abort().\n");

    std::string logpath = "/tmp/Xorg-sigsegv.log";
    XServer server;
    server.SetOption("-logfile", logpath);
    server.Start();

    ASSERT_EQ(server.GetState(), xorg::testing::Process::RUNNING);

    /* We connect and run XSync, this usually takes long enough for some
       devices to come up. That way, input devices are part of the close
       down procedure */
    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    XSync(dpy, False);

    kill(server.Pid(), SIGSEGV);

    int status = 1;
    while(WIFEXITED(status) == false)
        if (waitpid(server.Pid(), &status, 0) == -1)
            break;

    ASSERT_TRUE(WIFSIGNALED(status));
    int termsig = WTERMSIG(status);
    ASSERT_EQ(termsig, SIGABRT)
        << "Expected SIGABRT, got " << termsig << " (" << strsignal(termsig) << ")";

    std::ifstream logfile(server.GetLogFilePath().c_str());
    std::string line;
    std::string signal_caught_msg("Caught signal");
    int signal_count = 0;
    ASSERT_TRUE(logfile.is_open());
    while(getline(logfile, line)) {
        if (line.find(signal_caught_msg) != std::string::npos)
            signal_count++;
    }

    ASSERT_EQ(signal_count, 1) << "Expected one signal to show up in the log\n";

    unlink(logpath.c_str());

    /* The XServer destructor tries to terminate the server and prints
       warnings if it fails */
    std::cout << "Note: below warnings about server termination failures server are false positives\n";
    server.Terminate(); /* explicitly terminate instead of destructor to
                           only get one warning */
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
