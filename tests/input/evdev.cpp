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

#include "xit-server-input-test.h"
#include "device-interface.h"
#include "helpers.h"

typedef std::pair <int, KeySym> Key_Pair;
typedef std::multimap<std::string, Key_Pair> Keys_Map;
typedef Keys_Map::iterator keys_mapIter;
typedef std::vector<Key_Pair> MultiMedia_Keys_Map;
typedef MultiMedia_Keys_Map::iterator multimediakeys_mapIter;

class EvdevKeyboardTest : public XITServerInputTest,
                          public DeviceInterface {
public:
    virtual void SetUp() {
        SetDevice("keyboards/AT-Translated-Set-2-Keyboard.desc");
        XITServerInputTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CoreKeyboard\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default mouse device to avoid server adding our device again */
        config.AddInputSection("mouse", "mouse-device",
                               "Option \"CorePointer\" \"on\"\n");
        config.WriteConfig();
    }
};

class EvdevXKBConfigRulesTest : public EvdevKeyboardTest {
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CoreKeyboard\" \"on\"\n"
                               "Option \"XKBrules\" \"xorg\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default mouse device to avoid server adding our device again */
        config.AddInputSection("mouse", "mouse-device",
                               "Option \"CorePointer\" \"on\"\n");
        config.WriteConfig();
    }
};

TEST_F(EvdevXKBConfigRulesTest, NoRuleChangeAllowed) {
    XORG_TESTCASE("Start server with 'xorg' ruleset\n"
                  "Verify evdev changes this to 'evdev'\n");


    ASSERT_TRUE(SearchFileForString(server.GetLogFilePath(), "Option \"xkb_rules\" \"evdev\""));
    ASSERT_FALSE(SearchFileForString(server.GetLogFilePath(), "Option \"xkb_rules\" \"xorg\""));
}

struct xkb_config {
    std::string option;         /* option set in the server */
    std::string parsed_option;  /* option actually parsed (server ignores _) */
    std::string value;
} xkb_configs[] = {
    { "xkb_layout", "xkb_layout", "de" },
    { "XkbLayout", "xkb_layout", "de" },
    { "xkb_model", "xkb_model", "pc105" },
    { "XkbModel", "xkb_model", "pc105" },
    { "xkb_variant", "xkb_variant", "dvorak" },
    { "XkbVariant", "xkb_variant", "dvorak" },
    { "xkb_Options", "xkb_options", "compose:caps" },
    { "XkbOptions", "xkb_options", "compose:caps" },

    /* test for empty options */
    { "xkb_layout", "xkb_layout", "" },
    { "XkbLayout", "xkb_layout", "" },
    { "xkb_model", "xkb_model", "" },
    { "XkbModel", "xkb_model", "" },
    { "xkb_variant", "xkb_variant", "" },
    { "XkbVariant", "xkb_variant", "" },
    { "xkb_options", "xkb_options", "" },
    { "XkbOptions", "xkb_options", "" },
};

void PrintTo(const struct xkb_config &xkb, ::std::ostream *os) {
    *os << "xkb: " << xkb.option << ": " << xkb.value;
}

class EvdevXKBConfigTest : public EvdevKeyboardTest,
                           public ::testing::WithParamInterface<struct xkb_config> {
    virtual void SetUpConfigAndLog() {
        struct xkb_config xkb = GetParam();

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CoreKeyboard\" \"on\"\n"
                               "Option \"" + xkb.option + "\" \"" + xkb.value + "\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default mouse device to avoid server adding our device again */
        config.AddInputSection("mouse", "mouse-device",
                               "Option \"CorePointer\" \"on\"\n");
        config.WriteConfig();
    }
};

TEST_P(EvdevXKBConfigTest, XKBOptionParseTest) {
    XORG_TESTCASE("Init device with XKB options set.\n"
                  "Ensure options show up in the serverlog as the driver parses them\n");

    struct xkb_config xkb = GetParam();

    if (xkb.value.length() > 0)
        ASSERT_TRUE(SearchFileForString(server.GetLogFilePath(), "Option \"" + xkb.parsed_option + "\" \"" + xkb.value +"\""));
    else /* error message is "requires a string value" but thanks to a typo
            we'd need regex to check for that. oh well */
        ASSERT_TRUE(SearchFileForString(server.GetLogFilePath(), "Option \"" + xkb.parsed_option + "\" requires"));
}

INSTANTIATE_TEST_CASE_P(, EvdevXKBConfigTest, ::testing::ValuesIn(xkb_configs));


/**
 * Evdev driver test for keyboard devices. Takes a string as parameter,
 * which is later used for the XkbLayout option.
 */
class EvdevXKBTest : public EvdevKeyboardTest,
                     public ::testing::WithParamInterface<std::string> {
public:
    /**
     * Initializes a standard keyboard device.
     */
    virtual void SetUp() {
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

        EvdevKeyboardTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    virtual void SetUpConfigAndLog() {

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
        config.WriteConfig();
    }

    protected:
    /**
     * List of evdev keysyms to X11 keysyms for each layout.
     */
    Keys_Map Keys;

    /**
     * List of evdev keysyms to X11 keysyms for multimedia keys
     */
    MultiMedia_Keys_Map Multimedia_Keys;
};


TEST_P(EvdevXKBTest, DeviceExists)
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

TEST_P(EvdevXKBTest, KeyboardLayout)
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

INSTANTIATE_TEST_CASE_P(, EvdevXKBTest, ::testing::Values("us", "de", "fr"));

/**
 * Evdev driver test for mouse devices.
 */
class EvdevMouseTest : public XITServerInputTest,
                       public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device with two wheels.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        XITServerInputTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CorePointer device based on
     * the evemu device.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "keyboard-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }

    virtual int RegisterXI2(int major, int minor)
    {
        return XITServerInputTest::RegisterXI2(2, 1);
    }
};

TEST_F(EvdevMouseTest, TerminateWithButtonDown)
{
    XORG_TESTCASE("Terminate server with button down.\n"
                  "http://patchwork.freedesktop.org/patch/12193/");

    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, 1);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(), ButtonPress, -1, -1, 1000));
    XEvent ev;
    XNextEvent(Display(), &ev);
    ASSERT_FALSE(XPending(Display()));
}

TEST_F(EvdevMouseTest, BtnReleaseMaskOnly)
{
    XORG_TESTCASE("Ensure button release event is delivered if"
                  "only the\n release mask is set (not the press mask)");
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonReleaseMask);
    XSync(Display(), False);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(), ButtonRelease, -1, -1, 1000));
    XEvent ev;
    XNextEvent(Display(), &ev);

    ASSERT_FALSE(XPending(Display()));
}

#ifdef HAVE_XI22
TEST_F(EvdevMouseTest, SmoothScrollingAvailable)
{
    ASSERT_GE(RegisterXI2(2, 1), 1) << "Smooth scrolling requires XI 2.1+";

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1) << "Failed to find device.";

    int ndevices;
    XIDeviceInfo *info = XIQueryDevice(Display(), deviceid, &ndevices);
    ASSERT_EQ(ndevices, 1);

    int nvaluators = 0;
    bool hscroll_class_found = false;
    bool vscroll_class_found = false;
    for (int i = 0; i < info->num_classes; i++) {
        XIAnyClassInfo *any = info->classes[i];
        if (any->type == XIScrollClass) {
            XIScrollClassInfo *scroll = reinterpret_cast<XIScrollClassInfo*>(any);
            switch(scroll->scroll_type) {
                case XIScrollTypeVertical:
                    vscroll_class_found = true;
                    ASSERT_EQ(scroll->increment, -1);
                    ASSERT_EQ(scroll->flags & XIScrollFlagPreferred, XIScrollFlagPreferred);
                    break;
                case XIScrollTypeHorizontal:
                    hscroll_class_found = true;
                    ASSERT_EQ(scroll->increment, 1);
                    break;
                default:
                    FAIL() << "Invalid scroll class: " << scroll->type;
            }
        } else if (any->type == XIValuatorClass) {
            nvaluators++;
        }
    }

#ifndef HAVE_RHEL6
    ASSERT_EQ(nvaluators, 4);
    ASSERT_TRUE(hscroll_class_found);
    ASSERT_TRUE(vscroll_class_found);
#else
    /* RHEL6 disables smooth scrolling */
    ASSERT_EQ(nvaluators, 2);
    ASSERT_FALSE(hscroll_class_found);
    ASSERT_FALSE(vscroll_class_found);
#endif

    XIFreeDeviceInfo(info);
}

TEST_F(EvdevMouseTest, SmoothScrolling)
{
    ASSERT_GE(RegisterXI2(2, 1), 1) << "Smooth scrolling requires XI 2.1+";

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_Motion);
    mask.mask = new unsigned char[mask.mask_len];
    XISetMask(mask.mask, XI_Motion);
    XISetMask(mask.mask, XI_ButtonPress);
    XISetMask(mask.mask, XI_ButtonRelease);
    XISelectEvents(Display(), DefaultRootWindow(Display()), &mask, 1);
    XSync(Display(), False);

    delete mask.mask;

    dev->PlayOne(EV_REL, REL_WHEEL, -1, true);

    XEvent ev;
    XIDeviceEvent *e;

#ifndef HAVE_RHEL6
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_Motion,
                                                           1000));
    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.xany.type, GenericEvent);
    ASSERT_EQ(ev.xcookie.evtype, XI_Motion);
    XGetEventData(Display(), &ev.xcookie);
    e = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);
    ASSERT_GE(e->valuators.mask_len, 1); /* one 4-byte unit */
    ASSERT_TRUE(XIMaskIsSet(e->valuators.mask, 3)); /* order of axes is x, y, hwheel, wheel */
    XFreeEventData(Display(), &ev.xcookie);
#endif

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonPress,
                                                           1000));
    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.xany.type, GenericEvent);
    ASSERT_EQ(ev.xcookie.evtype, XI_ButtonPress);
    XGetEventData(Display(), &ev.xcookie);
    e = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);
    ASSERT_EQ(e->detail, 5);
#ifndef HAVE_RHEL6
    ASSERT_EQ(e->flags & XIPointerEmulated, XIPointerEmulated);
#else
    ASSERT_EQ(e->flags & XIPointerEmulated, 0);
#endif
    XFreeEventData(Display(), &ev.xcookie);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonRelease,
                                                           1000));
    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.xany.type, GenericEvent);
    ASSERT_EQ(ev.xcookie.evtype, XI_ButtonRelease);
    XGetEventData(Display(), &ev.xcookie);
    e = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);
    ASSERT_EQ(e->detail, 5);
#ifndef HAVE_RHEL6
    ASSERT_EQ(e->flags & XIPointerEmulated, XIPointerEmulated);
#else
    ASSERT_EQ(e->flags & XIPointerEmulated, 0);
#endif
    XFreeEventData(Display(), &ev.xcookie);

    ASSERT_FALSE(XPending(Display()));
}

#endif /* HAVE_XI22 */

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


TEST_F(EvdevMouseTest, ScrollWheel)
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

TEST_F(EvdevMouseTest, DevNode)
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

TEST_F(EvdevMouseTest, MiddleButtonEmulation)
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
class EvdevButtonMappingTest : public EvdevMouseTest,
                               public ::testing::WithParamInterface<Mapping> {
public:
    /**
     * Set up a config for a single evdev CorePointer device with
     * Option "ButtonMapping" based on the Mapping provided by GetParam()
     */
    virtual void SetUpConfigAndLog() {

        const Mapping mapping = GetParam();
        std::stringstream ss;
        for (int i = 0; i < mapping.nmap; i++) {
            ss << mapping.map[i];
            if (i < mapping.nmap - 1)
                ss << " ";
        }


        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\""
                               "Option \"ButtonMapping\" \"" + ss.str() + "\"");
        config.WriteConfig();
    }
};

TEST_P(EvdevButtonMappingTest, ButtonMapping)
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

INSTANTIATE_TEST_CASE_P(, EvdevButtonMappingTest,
                        ::testing::ValuesIn(mappings));

/**
 * Button mapping test for invalid button mappings the evdev driver. Takes a
 * string as input parameter.
 */
class EvdevInvalidButtonMappingTest : public EvdevMouseTest,
                                      public ::testing::WithParamInterface<std::string> {
public:
    /**
     * Set up a config for a single evdev CorePointer device with
     * Option "ButtonMapping" based on the string provided by GetParam()
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\""
                               "Option \"ButtonMapping\" \"" + GetParam() + "\"");
        config.WriteConfig();
    }
};

TEST_P(EvdevInvalidButtonMappingTest, InvalidButtonMapping)
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
class EvdevFloatingSlaveTest : public EvdevMouseTest {
public:
    /**
     * Set up a config for a single evdev Floating device.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"Floating\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        config.WriteConfig();
    }
};

TEST_F(EvdevFloatingSlaveTest, FloatingDevice)
{
    XORG_TESTCASE("Check that the server does not crash for a device\n"
                  "with Option Floating set.\n"
                  "xorg-server-1.12.99.903-49-gd53e6e0\n");
    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    int ndevices;
    XIDeviceInfo *info = XIQueryDevice(Display(), deviceid, &ndevices);
    ASSERT_EQ(ndevices, 1);

    ASSERT_EQ(info->use, XIFloatingSlave);

    XIFreeDeviceInfo(info);

    dev->PlayOne(EV_REL, ABS_X, 10, 1);
    dev->PlayOne(EV_REL, ABS_X, 4, 1);

    XSync(Display(), False);
    ASSERT_FALSE(XPending(Display()));

    ASSERT_EQ(server.GetState(), xorg::testing::Process::RUNNING);
}


INSTANTIATE_TEST_CASE_P(, EvdevInvalidButtonMappingTest,
                        ::testing::Values(" ", "a", "64", "1 2 ", "-1",
                                          "1 a", "1 55", "1 -2"));

class EvdevJoystickTest : public EvdevMouseTest {
    virtual void SetUp() {
        SetDevice("joysticks/Sony-PLAYSTATION(R)3-Controller.desc");
        XITServerInputTest::SetUp();
    }
};

TEST_F(EvdevJoystickTest, MTAxesNoButtons)
{
    XORG_TESTCASE("Set up a device with MT axes and only buttons > BTN_JOYSTICK\n"
                  "Expect the device to _not_ be added.\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=58967");


    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    XIEventMask mask;
    mask.mask_len = XIMaskLen(XI_ButtonRelease);
    mask.mask = new unsigned char[mask.mask_len]();
    mask.deviceid = XIAllMasterDevices;

    XISetMask(mask.mask, XI_ButtonPress);
    XISetMask(mask.mask, XI_ButtonRelease);

    XISelectEvents(Display(), DefaultRootWindow(Display()), &mask, 1);


    XIDeviceInfo *info;
    int ndevices;

    info = XIQueryDevice(Display(), deviceid, &ndevices);
    ASSERT_EQ(ndevices, 1);
    ASSERT_TRUE(info);
    ASSERT_GT(info->num_classes, 2);
    ASSERT_EQ(info->deviceid, deviceid);

    bool btn_class_found = false;
    for (int i = 0; i < info->num_classes; i++) {
        if (info->classes[i]->type == XIButtonClass) {
            XIButtonClassInfo *b = reinterpret_cast<XIButtonClassInfo*>(info->classes[i]);
            ASSERT_EQ(b->num_buttons, 5); /* we add scrolling */
            btn_class_found = true;
            break;
        }
    }

    ASSERT_TRUE(btn_class_found);

    XIFreeDeviceInfo(info);

    /*
        Playing an event would trigger a bug.
        [ 97436.293] (EE) BUG: triggered 'if (!b || !v)'
        [ 97436.293] (EE) BUG: exevents.c:929 in UpdateDeviceState()
    */

    dev->PlayOne(EV_ABS, ABS_MT_POSITION_X, 100);
    dev->PlayOne(EV_ABS, ABS_MT_POSITION_Y, 100);
    dev->PlayOne(EV_ABS, ABS_MT_TRACKING_ID, 1);
    dev->PlayOne(EV_ABS, ABS_MT_SLOT, 1, true);

    dev->PlayOne(EV_ABS, ABS_MT_TRACKING_ID, -1, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(), GenericEvent, xi2_opcode, XI_ButtonPress));
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
