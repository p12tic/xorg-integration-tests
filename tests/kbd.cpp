#include <stdexcept>
#include <xorg/gtest/xorg-gtest.h>
#include <linux/input.h>

#include <X11/Xlib.h>
#define XK_LATIN1
#include <X11/keysymdef.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"

class KeyboardDriverTest : public InputDriverTest {
    virtual void SetUp() {

        dev = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "keyboards/AT Translated Set 2 Keyboard.desc"
                    )
                );

        InputDriverTest::SetUp();
    }

    virtual void SetUpConfigAndLog(const std::string &prefix) {
        server.SetOption("-logfile", "/tmp/Xorg-kbd-driver.log");
        server.SetOption("-config", "/tmp/kbd-driver.conf");

        /* we don't use the dummy driver here, for some reason we won't get
         * key events with it */
        config.AddInputSection("kbd", "--device--", "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig("/tmp/kbd-driver.conf");
    }

    protected:
    std::auto_ptr<xorg::testing::evemu::Device> dev;
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

TEST_F(KeyboardDriverTest, QWERTY)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), KeyPressMask | KeyReleaseMask);
    /* the server takes a while to start up but the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());

    dev->PlayOne(EV_KEY, KEY_Q, 1, 1);
    dev->PlayOne(EV_KEY, KEY_Q, 0, 1);

    XSync(Display(), False);
    ASSERT_NE(XPending(Display()), 0);

    XEvent press;
    XNextEvent(Display(), &press);

    KeySym sym = XKeycodeToKeysym(Display(), press.xkey.keycode, 0);
    ASSERT_EQ(sym, XK_q);
}


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
