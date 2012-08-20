#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdexcept>
#include <map>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>
#include <linux/input.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#define XK_LATIN1
#include <X11/keysymdef.h>
#include <X11/XF86keysym.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"
#include "helpers.h"

typedef std::pair <int, KeySym> Key_Pair;
typedef std::multimap<std::string, Key_Pair> Keys_Map;
typedef Keys_Map::iterator keys_mapIter;
typedef std::vector<Key_Pair> MultiMedia_Keys_Map;
typedef MultiMedia_Keys_Map::iterator multimediakeys_mapIter;

/**
 * Evdev driver test for keyboard devices. Takes a string as parameter,
 * which is later used for the XkbLayout option.
 */
class EvdevDriverXKBTest : public InputDriverTest,
                           public ::testing::WithParamInterface<std::string> {
    /**
     * Initializes a standard keyboard device.
     */
    virtual void SetUp() {

        dev = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "keyboards/AT-Translated-Set-2-Keyboard.desc"
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

        // Define a vector of pair to hold each key/keysym for multimedia
        Multimedia_Keys.push_back (Key_Pair (KEY_MUTE,           XF86XK_AudioMute));
        Multimedia_Keys.push_back (Key_Pair (KEY_VOLUMEUP,       XF86XK_AudioRaiseVolume));
        Multimedia_Keys.push_back (Key_Pair (KEY_VOLUMEDOWN,     XF86XK_AudioLowerVolume));
        Multimedia_Keys.push_back (Key_Pair (KEY_PLAYPAUSE,      XF86XK_AudioPlay));
        Multimedia_Keys.push_back (Key_Pair (KEY_NEXTSONG,       XF86XK_AudioNext));
        Multimedia_Keys.push_back (Key_Pair (KEY_PREVIOUSSONG,   XF86XK_AudioPrev));

        InputDriverTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    virtual void SetUpConfigAndLog(const std::string &param) {
        server.SetOption("-logfile", "/tmp/Xorg-evdev-driver-xkb.log");
        server.SetOption("-config", "/tmp/evdev-driver-xkb.conf");

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CoreKeyboard\" \"on\"\n"
                               "Option \"XkbRules\"   \"xorg\"\n"
                               "Option \"XkbModel\"   \"dellusbmm\"\n"
                               "Option \"XkbLayout\"  \""+ GetParam() + "\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default mouse device to avoid server adding our device again */
        config.AddInputSection("mouse", "mouse-device",
                               "Option \"CorePointer\" \"on\"\n");
        config.WriteConfig("/tmp/evdev-driver-xkb.conf");
    }

    protected:
    /**
     * The evemu device to generate events.
     */
    std::auto_ptr<xorg::testing::evemu::Device> dev;

    /**
     * List of evdev keysyms to X11 keysyms for each layout.
     */
    Keys_Map Keys;

    /**
     * List of evdev keysyms to X11 keysyms for multimedia keys
     */
    MultiMedia_Keys_Map Multimedia_Keys;
};


TEST_P(EvdevDriverXKBTest, DeviceExists)
{
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--"), 1);
}

void play_key_pair (::Display *display, xorg::testing::evemu::Device *dev, Key_Pair pair)
{
    dev->PlayOne(EV_KEY, pair.first, 1, 1);
    dev->PlayOne(EV_KEY, pair.first, 0, 1);

    XSync(display, False);
    ASSERT_NE(XPending(display), 0) << "No event pending" << std::endl;

    XEvent press;
    XNextEvent(display, &press);

    KeySym sym = XKeycodeToKeysym(display, press.xkey.keycode, 0);
    ASSERT_NE((KeySym)NoSymbol, sym) << "No keysym for keycode " << press.xkey.keycode << std::endl;
    ASSERT_EQ(pair.second, sym) << "Keysym not matching for keycode " << press.xkey.keycode << std::endl;

    XSync(display, False);
    while (XPending(display))
      XNextEvent(display, &press);
}

TEST_P(EvdevDriverXKBTest, KeyboardLayout)
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
        play_key_pair (Display(), dev.get(), (*it).second);

    // Now test multimedia keys
    multimediakeys_mapIter m_it;
    for (m_it = Multimedia_Keys.begin(); m_it != Multimedia_Keys.end(); m_it++)
        play_key_pair (Display(), dev.get(), (*m_it));
}

INSTANTIATE_TEST_CASE_P(, EvdevDriverXKBTest, ::testing::Values("us", "de", "fr"));

/**
 * Evdev driver test for mouse devices.
 */
class EvdevDriverMouseTest : public InputDriverTest {
public:
    /**
     * Initializes a standard mouse device with two wheels.
     */
    virtual void SetUp() {
        dev = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc"
                    )
                );
        InputDriverTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CorePointer device based on
     * the evemu device.
     */
    virtual void SetUpConfigAndLog(const std::string &param) {
        server.SetOption("-logfile", "/tmp/Xorg-evdev-driver-mouse.log");
        server.SetOption("-config", "/tmp/evdev-driver-mouse.conf");

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "keyboard-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig("/tmp/evdev-driver-mouse.conf");
    }

protected:
    /**
     * The evemu device to generate events.
     */
    std::auto_ptr<xorg::testing::evemu::Device> dev;
};

void scroll_wheel_event(::Display *display,
                        xorg::testing::evemu::Device *dev,
                        int axis, int value, unsigned int button) {

    dev->PlayOne(EV_REL, axis, value, 1);

    XSync(display, False);

    ASSERT_NE(XPending(display), 0) << "No event pending" << std::endl;
    XEvent btn;
    int nevents = 0;
    while(XPending(display)) {
        XNextEvent(display, &btn);

        ASSERT_EQ(btn.xbutton.type, ButtonPress);
        ASSERT_EQ(btn.xbutton.button, button);

        XNextEvent(display, &btn);
        ASSERT_EQ(btn.xbutton.type, ButtonRelease);
        ASSERT_EQ(btn.xbutton.button, button);

        nevents++;
    }

    ASSERT_EQ(nevents, abs(value));
}


TEST_F(EvdevDriverMouseTest, ScrollWheel)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    /* the server takes a while to start up bust the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());

    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, 1, 4);
    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, 2, 4);
    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, 3, 4);

    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, -1, 5);
    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, -2, 5);
    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, -3, 5);

    scroll_wheel_event(Display(), dev.get(), REL_HWHEEL, 1, 7);
    scroll_wheel_event(Display(), dev.get(), REL_HWHEEL, 2, 7);
    scroll_wheel_event(Display(), dev.get(), REL_HWHEEL, 3, 7);

    scroll_wheel_event(Display(), dev.get(), REL_HWHEEL, -1, 6);
    scroll_wheel_event(Display(), dev.get(), REL_HWHEEL, -2, 6);
    scroll_wheel_event(Display(), dev.get(), REL_HWHEEL, -3, 6);
}

TEST_F(EvdevDriverMouseTest, DevNode)
{
    Atom node_prop = XInternAtom(Display(), "Device Node", True);

    ASSERT_NE(node_prop, (Atom)None) << "This requires server 1.11";

    int deviceid = -1;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1) << "Failed to find device.";

    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    char *data;
    XIGetProperty(Display(), deviceid, node_prop, 0, 100, False,
                  AnyPropertyType, &type, &format, &nitems, &bytes_after,
                  (unsigned char**)&data);
    ASSERT_EQ(type, XA_STRING);

    std::string devnode(data);
    ASSERT_EQ(devnode.compare(dev->GetDeviceNode()), 0);

    XFree(data);
}

TEST_F(EvdevDriverMouseTest, MiddleButtonEmulation)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XFlush(Display());

    Atom mb_prop = XInternAtom(Display(), "Evdev Middle Button Emulation", True);

    ASSERT_NE(mb_prop, (Atom)None);

    int deviceid = -1;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1) << "Failed to find device.";

    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *data;
    XIGetProperty(Display(), deviceid, mb_prop, 0, 100, False,
                  AnyPropertyType, &type, &format, &nitems, &bytes_after,
                  &data);
    ASSERT_EQ(type, XA_INTEGER);
    ASSERT_EQ(format, 8);
    ASSERT_EQ(nitems, 1U);

    /* enable mb emulation */
    *data = 1;
    XIChangeProperty(Display(), deviceid, mb_prop, type, format,
                     PropModeReplace, data, 1);
    XSync(Display(), False);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, 1);
    dev->PlayOne(EV_KEY, BTN_RIGHT, 1, 1);
    dev->PlayOne(EV_KEY, BTN_RIGHT, 0, 1);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, 1);

    XSync(Display(), False);

    ASSERT_NE(XPending(Display()), 0) << "No event pending" << std::endl;
    XEvent btn;

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 2U);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, 2U);

    ASSERT_EQ(XPending(Display()), 0) << "Events pending when there should be none" << std::endl;

    /* disable mb emulation */
    *data = 0;
    XIChangeProperty(Display(), deviceid, mb_prop, type, format,
                     PropModeReplace, data, 1);
    XSync(Display(), False);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, 1);
    dev->PlayOne(EV_KEY, BTN_RIGHT, 1, 1);
    dev->PlayOne(EV_KEY, BTN_RIGHT, 0, 1);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, 1);

    XSync(Display(), False);

    ASSERT_NE(XPending(Display()), 0) << "No event pending" << std::endl;

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 1U);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 3U);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, 3U);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, 1U);

    ASSERT_EQ(XPending(Display()), 0) << "Events pending when there should be none" << std::endl;

    XFree(data);
}

void button_event(::Display *display,
                  xorg::testing::evemu::Device *dev,
                  int button, unsigned int logical) {

    dev->PlayOne(EV_KEY, button, 1, true);
    dev->PlayOne(EV_KEY, button, 0, true);

    XSync(display, False);

    ASSERT_NE(XPending(display), 0) << "No event pending" << std::endl;

    XEvent btn;
    XNextEvent(display, &btn);

    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, logical);

    XNextEvent(display, &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, logical);

    ASSERT_EQ(XPending(display), 0) << "Events pending when there should be none" << std::endl;
}

#ifndef DOXYGEN_IGNORE_THIS

typedef struct {
    int map[8];
    int nmap;
} Mapping;

void PrintTo(const Mapping &m, ::std::ostream *os) {
    for (int i = 0; i < m.nmap; i++)
        *os << m.map[i] << " ";
}

#endif /* DOXYGEN_IGNORE_THIS */

/**
 * Button mapping test for the evdev driver. Takes a struct Mapping as input
 * parameter:
 */
class EvdevDriverButtonMappingTest : public EvdevDriverMouseTest,
                                     public ::testing::WithParamInterface<Mapping> {
public:
    /**
     * Set up a config for a single evdev CorePointer device with
     * Option "ButtonMapping" based on the Mapping provided by GetParam()
     */
    virtual void SetUpConfigAndLog(const std::string &param) {

        const Mapping mapping = GetParam();
        std::stringstream ss;
        for (int i = 0; i < mapping.nmap; i++) {
            ss << mapping.map[i];
            if (i < mapping.nmap - 1)
                ss << " ";
        }

        server.SetOption("-logfile", "/tmp/Xorg-evdev-driver-buttonmapping.log");
        server.SetOption("-config", "/tmp/evdev-driver-buttonmapping.conf");

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\""
                               "Option \"ButtonMapping\" \"" + ss.str() + "\"");
        config.WriteConfig("/tmp/evdev-driver-buttonmapping.conf");
    }
};

TEST_P(EvdevDriverButtonMappingTest, ButtonMapping)
{
    Mapping mapping = GetParam();

    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XFlush(Display());

    XSync(Display(), True);

    button_event(Display(), dev.get(), BTN_LEFT, mapping.map[0]);
    button_event(Display(), dev.get(), BTN_MIDDLE, mapping.map[1]);
    button_event(Display(), dev.get(), BTN_RIGHT, mapping.map[2]);
}

Mapping mappings[] = {
    { {1, 2, 3}, 3 },
    { {3, 1, 1}, 3 },
    { {1, 3, 1}, 3 },
    { {3, 1, 2}, 3 },
};

INSTANTIATE_TEST_CASE_P(, EvdevDriverButtonMappingTest,
                        ::testing::ValuesIn(mappings));

/**
 * Button mapping test for invalid button mappings the evdev driver. Takes a
 * string as input parameter.
 */
class EvdevDriverInvalidButtonMappingTest : public EvdevDriverMouseTest,
                                            public ::testing::WithParamInterface<std::string> {
public:
    /**
     * Set up a config for a single evdev CorePointer device with
     * Option "ButtonMapping" based on the string provided by GetParam()
     */
    virtual void SetUpConfigAndLog(const std::string &param) {
        std::string log_file = "/tmp/Xorg-evdev-driver-buttonmapping-invalid.log";
        server.SetOption("-logfile", log_file);
        server.SetOption("-config", "/tmp/evdev-driver-buttonmapping-invalid.conf");

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\""
                               "Option \"ButtonMapping\" \"" + GetParam() + "\"");
        config.WriteConfig("/tmp/evdev-driver-buttonmapping-invalid.conf");
    }
};

TEST_P(EvdevDriverInvalidButtonMappingTest, InvalidButtonMapping)
{
    std::ifstream in_file(server.GetLogFilePath().c_str());
    std::string line;
    std::string error_msg("Invalid button mapping.");

    // grep for error message in the log file...
    if(in_file.is_open())
    {
        while (getline (in_file, line))
        {
            size_t found = line.find(error_msg);
            if (found != std::string::npos)
                return;
        }
    }
    FAIL() << "Invalid button mapping message not found in log";
}


/**
 * Test for the server honouring Option Floating "on".
 */
class EvdevDriverFloatingSlaveTest : public EvdevDriverMouseTest {
public:
    /**
     * Set up a config for a single evdev Floating device.
     */
    virtual void SetUpConfigAndLog(const std::string &param) {
        std::string log_file = "/tmp/Xorg-evdev-driver-floating.log";
        server.SetOption("-logfile", log_file);
        server.SetOption("-config", "/tmp/evdev-driver-floating.conf");

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"Floating\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        config.WriteConfig("/tmp/evdev-driver-floating.conf");
    }
};

TEST_F(EvdevDriverFloatingSlaveTest, FloatingDevice)
{
    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    int ndevices;
    XIDeviceInfo *info = XIQueryDevice(Display(), deviceid, &ndevices);
    ASSERT_EQ(ndevices, 1);

    ASSERT_EQ(info->use, XIFloatingSlave);

    XIFreeDeviceInfo(info);

    pid_t pid = fork();
    if (pid == 0) {
        dev->PlayOne(EV_REL, ABS_X, 10, 1);
        dev->PlayOne(EV_REL, ABS_X, 4, 1);

        XSync(Display(), False);
        ASSERT_FALSE(xorg::testing::XServer::WaitForEventOfType(Display(), MotionNotify, -1, -1, 1000));

        ASSERT_EQ(server.GetState(), xorg::testing::Process::RUNNING);
    } else {
        int status;
        waitpid(pid, &status, 0);

        ASSERT_TRUE(WIFEXITED(status));
        ASSERT_EQ(WEXITSTATUS(status), 0);
    }
}


INSTANTIATE_TEST_CASE_P(, EvdevDriverInvalidButtonMappingTest,
                        ::testing::Values(" ", "a", "64", "1 2 ", "-1",
                                          "1 a", "1 55", "1 -2"));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
