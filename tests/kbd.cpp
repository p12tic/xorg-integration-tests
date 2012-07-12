#include <stdexcept>
#include <map>
#include <xorg/gtest/xorg-gtest.h>
#include <linux/input.h>

#include <X11/Xlib.h>
#define XK_LATIN1
#include <X11/keysymdef.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"

typedef std::pair <int, KeySym> Key_Pair;
typedef std::multimap<std::string, Key_Pair> Keys_Map;
typedef Keys_Map::iterator keys_mapIter;

class KeyboardDriverTest : public InputDriverTest {
    virtual void SetUp() {

        dev = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "keyboards/AT Translated Set 2 Keyboard.desc"
                    )
                );

        // Define a map of pair to hold each key/keysym per layout
        // US, QWERTY => qwerty
        Keys.insert (std::pair<std::string, Key_Pair> ("us", Key_Pair (KEY_Q, XK_q)));
        Keys.insert (std::pair<std::string, Key_Pair> ("us", Key_Pair (KEY_W, XK_w)));
        Keys.insert (std::pair<std::string, Key_Pair> ("us", Key_Pair (KEY_E, XK_e)));
        Keys.insert (std::pair<std::string, Key_Pair> ("us", Key_Pair (KEY_R, XK_r)));
        Keys.insert (std::pair<std::string, Key_Pair> ("us", Key_Pair (KEY_T, XK_t)));
        Keys.insert (std::pair<std::string, Key_Pair> ("us", Key_Pair (KEY_Y, XK_y)));
        // German, QWERTY => qwertz
        Keys.insert (std::pair<std::string, Key_Pair> ("de", Key_Pair (KEY_Q, XK_q)));
        Keys.insert (std::pair<std::string, Key_Pair> ("de", Key_Pair (KEY_W, XK_w)));
        Keys.insert (std::pair<std::string, Key_Pair> ("de", Key_Pair (KEY_E, XK_e)));
        Keys.insert (std::pair<std::string, Key_Pair> ("de", Key_Pair (KEY_R, XK_r)));
        Keys.insert (std::pair<std::string, Key_Pair> ("de", Key_Pair (KEY_T, XK_t)));
        Keys.insert (std::pair<std::string, Key_Pair> ("de", Key_Pair (KEY_Y, XK_z)));
        // French, QWERTY => azerty
        Keys.insert (std::pair<std::string, Key_Pair> ("fr", Key_Pair (KEY_Q, XK_a)));
        Keys.insert (std::pair<std::string, Key_Pair> ("fr", Key_Pair (KEY_W, XK_z)));
        Keys.insert (std::pair<std::string, Key_Pair> ("fr", Key_Pair (KEY_E, XK_e)));
        Keys.insert (std::pair<std::string, Key_Pair> ("fr", Key_Pair (KEY_R, XK_r)));
        Keys.insert (std::pair<std::string, Key_Pair> ("fr", Key_Pair (KEY_T, XK_t)));
        Keys.insert (std::pair<std::string, Key_Pair> ("fr", Key_Pair (KEY_Y, XK_y)));

        InputDriverTest::SetUp();
    }

    virtual void SetUpConfigAndLog(const std::string &prefix) {
        server.SetOption("-logfile", "/tmp/Xorg-kbd-driver.log");
        server.SetOption("-config", "/tmp/kbd-driver.conf");

        /* we don't use the dummy driver here, for some reason we won't get
         * key events with it */
        config.AddInputSection("kbd", "--device--",
                               "Option \"CoreKeyboard\" \"on\"\n"
                               "Option \"XkbRules\"   \"xorg\"\n"
                               "Option \"XkbModel\"   \"pc104\"\n"
                               "Option \"XkbLayout\"  \""+ prefix + "\"\n");
        config.WriteConfig("/tmp/kbd-driver.conf");
    }

    protected:
    std::auto_ptr<xorg::testing::evemu::Device> dev;
    Keys_Map Keys;
};


TEST_P(KeyboardDriverTest, DeviceExists)
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

TEST_P(KeyboardDriverTest, KeyboardLayout)
{
    std::string layout = GetParam();

    XSelectInput(Display(), DefaultRootWindow(Display()), KeyPressMask | KeyReleaseMask);
    /* the server takes a while to start up bust the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());

    keys_mapIter it;
    std::pair<keys_mapIter, keys_mapIter> keyRange = Keys.equal_range(layout);
    for (it = keyRange.first;  it != keyRange.second;  ++it)
    {
        Key_Pair pair = (*it).second;

        dev->PlayOne(EV_KEY, pair.first, 1, 1);
        dev->PlayOne(EV_KEY, pair.first, 0, 1);

        XSync(Display(), False);
        ASSERT_NE(XPending(Display()), 0) << "No event pending" << std::endl;

        XEvent press;
        XNextEvent(Display(), &press);

        KeySym sym = XKeycodeToKeysym(Display(), press.xkey.keycode, 0);
        ASSERT_EQ(pair.second, sym) << "Keysym not matching" << std::endl;

        XSync(Display(), False);
        while (XPending(Display()))
            XNextEvent(Display(), &press);
    }
}

INSTANTIATE_TEST_CASE_P(, KeyboardDriverTest, ::testing::Values("us", "de", "fr"));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
