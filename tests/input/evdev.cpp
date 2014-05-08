/*
 * Copyright © 2012-2013 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <math.h>
#include <stdexcept>
#include <map>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>
#include <linux/input.h>
#include <tr1/tuple>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#define XK_LATIN1
#include <X11/keysymdef.h>
#include <X11/XF86keysym.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "xit-server-input-test.h"
#include "xit-event.h"
#include "xit-property.h"
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
                               "Option \"GrabDevice\" \"on\""
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
                               "Option \"GrabDevice\" \"on\""
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
                               "Option \"GrabDevice\" \"on\""
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
                               "Option \"GrabDevice\" \"on\""
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

        xi2_major_minimum = 2;
        xi2_minor_minimum = 1;

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
                               "Option \"GrabDevice\" \"on\""
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "keyboard-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
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

TEST_F(EvdevMouseTest, WheelEmulationZeroIsBadValue)
{
    XORG_TESTCASE("Set wheel emulation intertia to 0\n"
                  "Expect BadValue\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=66125");

    ::Display *dpy = Display();
    int deviceid;
    ASSERT_TRUE(FindInputDeviceByName(Display(), "--device--", &deviceid));


    ASSERT_PROPERTY(short, emulation, dpy, deviceid, "Evdev Wheel Emulation Inertia");
    emulation.data[0] = 0;

    SetErrorTrap(dpy);
    emulation.Update();
    ASSERT_ERROR(ReleaseErrorTrap(dpy), BadValue);
}

#ifdef HAVE_XI22
TEST_F(EvdevMouseTest, SmoothScrollingAvailable)
{
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
                               "Option \"GrabDevice\" \"on\""
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
                               "Option \"GrabDevice\" \"on\""
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
                               "Option \"GrabDevice\" \"on\""
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

class EvdevTrackballTest : public EvdevMouseTest {
    virtual void SetUp() {
        SetDevice("mice/Logitech-USB-Trackball.desc");
        XITServerInputTest::SetUp();
    }

    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "Logitech USB Trackball",
                               "Option \"Floating\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\""
                               "Option \"TypeName\" \"TRACKBALL\""
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        config.WriteConfig();
    }
};

TEST_F(EvdevTrackballTest, TypeIsXI_TRACKBALL)
{
    XORG_TESTCASE("A trackball should be of type XI_TRACKBALL.\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=55867");

    ::Display *dpy = Display();

    Atom trackball_prop = XInternAtom(dpy, XI_TRACKBALL, True);
    ASSERT_NE(trackball_prop, (Atom)None);

    int ndevices;
    XDeviceInfo *info = XListInputDevices(dpy, &ndevices);

    for (int i = 0; i < ndevices; i++) {
        XDeviceInfo *di = &info[i];
        if (strcmp(di->name, "Logitech USB Trackball") == 0) {
            ASSERT_EQ(di->type, trackball_prop);
            return;
        }
    }

    FAIL() << "Failed to find device";
}

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
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 0);
}

#ifdef HAVE_XI22
class EvdevTouchTest : public XITServerInputTest,
                       public DeviceInterface {
protected:
    virtual void SetUp() {
        SetDevice("tablets/N-Trig-MultiTouch.desc");

        xi2_major_minimum = 2;
        xi2_minor_minimum = 2;

        XITServerInputTest::SetUp();
    }

    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"\n"
                               "Option \"GrabDevice\" \"on\"");
        config.WriteConfig();
    }

    virtual void TouchBegin(int x, int y) {
        dev->PlayOne(EV_KEY, BTN_TOUCH, 1);
        TouchUpdate(x, y);
    }

    virtual void TouchUpdate(int x, int y) {
        dev->PlayOne(EV_ABS, ABS_X, x);
        dev->PlayOne(EV_ABS, ABS_Y, y);
        dev->PlayOne(EV_ABS, ABS_MT_POSITION_X, x);
        dev->PlayOne(EV_ABS, ABS_MT_POSITION_Y, y);
        /* same values as the recordings file */
        dev->PlayOne(EV_ABS, ABS_MT_ORIENTATION, 0);
        dev->PlayOne(EV_ABS, ABS_MT_TOUCH_MAJOR, 468);
        dev->PlayOne(EV_ABS, ABS_MT_TOUCH_MINOR, 306);
        dev->PlayOne(EV_SYN, SYN_MT_REPORT, 0);
        dev->PlayOne(EV_SYN, SYN_REPORT, 0);
    }

    virtual void TouchEnd() {
        dev->PlayOne(EV_KEY, BTN_TOUCH, 0);
        dev->PlayOne(EV_SYN, SYN_REPORT, 0);
    }

};

enum directions {
    NONE,
    CW, /* top screen edge is right */
    INVERT, /* upside-down */
    CCW, /* top screen edge is left */
};

enum config_type {
    PROPERTY,
    XORG_CONF
};

class EvdevTouchRotationTest : public EvdevTouchTest,
                               public
                               ::testing::WithParamInterface<std::tr1::tuple<enum config_type, enum directions> > {
public:
    void GetRotationSettings(enum directions dir, unsigned char *swap_axes,
                             unsigned char *invert_x, unsigned char *invert_y)
    {
        switch(dir) {
            case NONE:
                *swap_axes = 0;
                *invert_x = 0;
                *invert_y = 0;
                break;
            case INVERT:
                *swap_axes = 0;
                *invert_x = 1;
                *invert_y = 1;
                break;
            case CW:
                *swap_axes = 1;
                *invert_x = 1;
                *invert_y = 0;
                break;
            case CCW:
                *swap_axes = 1;
                *invert_x = 0;
                *invert_y = 1;
                break;
        }
    }

    virtual void SetUpConfigAndLog() {
        std::tr1::tuple<enum config_type, enum directions> t = GetParam();
        enum config_type config_type = std::tr1::get<0>(t);
        enum directions dir = std::tr1::get<1>(t);

        unsigned char swap_axes = 0, invert_x = 0, invert_y = 0;
        if (config_type == XORG_CONF)
            GetRotationSettings(dir, &swap_axes, &invert_x, &invert_y);

        std::string sw = swap_axes ? "on" : "off";
        std::string ix = invert_x ? "on" : "off";
        std::string iy = invert_y ? "on" : "off";

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"SwapAxes\" \"" + sw + "\"\n" +
                               "Option \"InvertX\" \"" + ix + "\"\n" +
                               "Option \"InvertY\" \"" + iy + "\"\n" +
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"\n"
                               "Option \"GrabDevice\" \"on\"");
        config.WriteConfig();
    }

    virtual void SetRotationProperty(::Display *dpy, int deviceid, enum directions dir)
    {
        Atom swap_axes_prop = XInternAtom(dpy, "Evdev Axes Swap", True);
        Atom invert_axes_prop = XInternAtom(dpy, "Evdev Axis Inversion", True);

        unsigned char swap;
        unsigned char inversion[2];

        GetRotationSettings(dir, &swap, &inversion[0], &inversion[1]);

        XIChangeProperty(dpy, deviceid, swap_axes_prop, XA_INTEGER, 8,
                         PropModeReplace, &swap, 1);
        XIChangeProperty(dpy, deviceid, invert_axes_prop, XA_INTEGER, 8,
                         PropModeReplace, inversion, 2);
        XSync(dpy, True);
    }
};

TEST_P(EvdevTouchRotationTest, AxisSwapInversion)
{
    std::tr1::tuple<enum config_type, enum directions> t = GetParam();
    enum config_type config_type = std::tr1::get<0>(t);
    enum directions dir = std::tr1::get<1>(t);

    XORG_TESTCASE("Init a touch device.\n"
                  "Set rotation/axis inversion properties\n"
                  "Send events\n"
                  "Ensure event coordinates are rotated\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=59340");

    std::string direction;
    switch (dir) {
        case NONE: direction = "none"; break;
        case CW: direction = "cw"; break;
        case CCW: direction = "ccw"; break;
        case INVERT: direction = "inverted"; break;
    }

    SCOPED_TRACE("Rotation is " + direction);

    ::Display *dpy = Display();

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--", &deviceid), 1);

    if (config_type == PROPERTY)
        SetRotationProperty(dpy, deviceid, dir);

    XIEventMask mask;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);
    XISelectEvents(dpy, DefaultRootWindow(dpy), &mask, 1);

    int w = DisplayWidth(dpy, DefaultScreen(dpy));
    int h = DisplayHeight(dpy, DefaultScreen(dpy));

    int devw = 9600, devh = 7200; /* from recordings file */

    TouchBegin(devw * 0.25, devh * 0.25);

    ASSERT_EVENT(XIDeviceEvent, begin, dpy, GenericEvent, xi2_opcode, XI_TouchBegin);

    /* scaling in the server may get us a pixel or so off */
#define EXPECT_APPROX(v, expected) \
    EXPECT_GE(v, expected - 1); \
    EXPECT_LE(v, expected + 1);

    /* x movement first */
    for (int i = 30; i < 75; i += 5) {
        double xpos = i/100.0;
        double x_expect = 0, y_expect = 0;

        TouchUpdate(devw * xpos, devh * 0.25);

        switch(dir) {
            case NONE:
                x_expect = w * xpos;
                y_expect = h * 0.25;
                break;
            case CW:
                x_expect = w * 0.75;
                y_expect = h * xpos;
                break;
            case CCW:
                x_expect = w * 0.25;
                y_expect = h * (1 - xpos);
                break;
            case INVERT:
                x_expect = w * (1 - xpos);
                y_expect = h * 0.75;
                break;
            default:
                FAIL();
        }


        ASSERT_EVENT(XIDeviceEvent, update, dpy, GenericEvent, xi2_opcode, XI_TouchUpdate);
        EXPECT_APPROX(update->root_x, x_expect);
        EXPECT_APPROX(update->root_y, y_expect);
    }

    /* y movement */
    for (int i = 30; i < 75; i += 5) {
        double ypos = i/100.0;
        double x_expect = 0, y_expect = 0;

        TouchUpdate(devw * 0.25, devh * ypos);

        switch(dir) {
            case NONE:
                x_expect = w * 0.25;
                y_expect = h * ypos;
                break;
            case CW:
                x_expect = w * (1 - ypos);
                y_expect = h * 0.25;
                break;
            case CCW:
                x_expect = w * ypos;
                y_expect = h * 0.75;
                break;
            case INVERT:
                x_expect = w * 0.75;
                y_expect = h * (1 - ypos);
                break;
            default:
                FAIL();
        }

        ASSERT_EVENT(XIDeviceEvent, update, dpy, GenericEvent, xi2_opcode, XI_TouchUpdate);
        EXPECT_APPROX(update->root_x, x_expect);
        EXPECT_APPROX(update->root_y, y_expect);
    }

    TouchEnd();
}
INSTANTIATE_TEST_CASE_P(, EvdevTouchRotationTest,
        ::testing::Combine(
            ::testing::Values(PROPERTY, XORG_CONF),
            ::testing::Values(NONE, CW, CCW, INVERT)));

#endif

class EvdevAxisLabelTest : public XITServerInputTest,
                              public DeviceInterface {
public:
    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.SetAutoAddDevices(true);
        config.WriteConfig();
    }

    std::vector<Atom> GetAxisLabels(::Display *dpy, int deviceid) {
        int ndevices = 0;
        XIDeviceInfo *info = XIQueryDevice(dpy, deviceid, &ndevices);

        EXPECT_EQ(ndevices, 1);

        std::vector<Atom> atoms;

        for (int i = 0; info && i < info->num_classes; i++) {
            if (info->classes[i]->type != XIValuatorClass)
                continue;

            XIValuatorClassInfo *v = reinterpret_cast<XIValuatorClassInfo*>(info->classes[i]);
            atoms.push_back(v->label);
        }

        return atoms;
    }

    std::vector<Atom> GetAtoms(::Display *dpy, std::vector<std::string> names) {
        std::vector<Atom> atoms;

        std::vector<std::string>::const_iterator it = names.begin();
        while(it != names.end()) {
            Atom a = XInternAtom(dpy, it->c_str(), True);
            EXPECT_NE(a, (Atom)None) << "for label " << *it;
            atoms.push_back(a);
            it++;
        }

        return atoms;
    }

    void SetUpDevice(::Display *dpy,
                     const std::string &recordings_file,
                     const std::string name,
                     int *deviceid) {
        *deviceid = -1;
        SetDevice(recordings_file);
        ASSERT_TRUE(WaitForDevice(name, 10000));
        ASSERT_EQ(1, FindInputDeviceByName(dpy, name, deviceid));
        ASSERT_NE(*deviceid, -1);
    }

    void CompareLabels(::Display *dpy, int deviceid, const char *axislabels[])
    {
        std::vector<std::string> strs;
        const char **l = axislabels;

        while(*l) {
            strs.push_back(*l);
            l++;
        }

        std::vector<Atom> expected_labels = GetAtoms(dpy, strs);
        std::vector<Atom> labels = GetAxisLabels(dpy, deviceid);

        ASSERT_EQ(labels.size(), expected_labels.size());

        std::vector<Atom>::const_iterator label = labels.begin();
        std::vector<Atom>::const_iterator expected = expected_labels.begin();

        while (label != labels.end()) {
            ASSERT_EQ(*label, *expected);
            label++;
            expected++;
        }
    }
};

TEST_F(EvdevAxisLabelTest, RelativeAxes)
{
    ::Display *dpy = Display();
    int deviceid;

    SetUpDevice(dpy, "mice/PIXART-USB-OPTICAL-MOUSE.desc", "PIXART USB OPTICAL MOUSE", &deviceid);
    ASSERT_NE(deviceid, 0);

    const char *l[] = {"Rel X", "Rel Y", "Rel Vert Wheel", NULL};

    CompareLabels(dpy, deviceid, l);

}

TEST_F(EvdevAxisLabelTest, AbsoluteAxes)
{
    ::Display *dpy = Display();
    int deviceid;

    SetUpDevice(dpy, "tablets/N-Trig-MultiTouch.desc", "N-Trig MultiTouch", &deviceid);
    ASSERT_NE(deviceid, 0);

    const char *l[] = {"Abs MT Position X", "Abs MT Position Y", "Abs Misc",
                       "Abs MT Touch Major", "Abs MT Touch Minor", "Abs MT Orientation", NULL};

    CompareLabels(dpy, deviceid, l);
}

TEST_F(EvdevAxisLabelTest, RelAndAbsoluteAxes)
{
    ::Display *dpy = Display();
    int deviceid;

    SetUpDevice(dpy, "tablets/QEMU-0.12.1-QEMU-USB-Tablet.desc", "QEMU 0.12.1 QEMU USB Tablet", &deviceid);
    ASSERT_NE(deviceid, 0);

    const char *l[] = {"Abs X", "Abs Y",  "Rel Vert Wheel", NULL};

    CompareLabels(dpy, deviceid, l);
}

class EvdevQEMUTabletTest : public EvdevMouseTest {
public:
    void SetUp(void) {
        SetDevice("tablets/QEMU-0.12.1-QEMU-USB-Tablet.desc");
        XITServerInputTest::SetUp();
    }
};
#if HAVE_XI22
TEST_F(EvdevQEMUTabletTest, HasScrollingAxes) {
    XORG_TESTCASE("Create QEMU virtual tablet device.\n"
                  "Ensure scrolling axes are present.\n");

    ::Display* dpy = Display();

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--", &deviceid), 1);

    int ndevices;
    XIDeviceInfo *info = XIQueryDevice(dpy, deviceid, &ndevices);
    ASSERT_EQ(ndevices, 1);

    bool scroll_found = false;
    for (int i = 0; i < info->num_classes; i++) {
        if (info->classes[i]->type != XIScrollClass)
            continue;

        scroll_found = true;
        XIScrollClassInfo *scroll = reinterpret_cast<XIScrollClassInfo*>(info->classes[i]);
        ASSERT_EQ(scroll->scroll_type, XIScrollTypeVertical);
    }

    ASSERT_TRUE(scroll_found);
}

TEST_F(EvdevQEMUTabletTest, SmoothScrollingWorks) {
    XORG_TESTCASE("Create QEMU virtual tablet device.\n"
                  "Send scroll wheel events.\n"
                  "Verify smooth scroll events are received on the right valuator\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=54387");

    ::Display* dpy = Display();

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--", &deviceid), 1);

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_Motion);
    mask.mask = new unsigned char[mask.mask_len]();

    XISetMask(mask.mask, XI_Motion);
    XISelectEvents(dpy, DefaultRootWindow(dpy), &mask , 1);

    dev->PlayOne(EV_REL, REL_WHEEL, -1, true);

    ASSERT_EVENT(XIDeviceEvent, down, dpy, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EQ(down->valuators.mask[0], 1 << 2); /* only scroll valuator moved */
    ASSERT_EQ(down->valuators.values[0], -1.0);

    dev->PlayOne(EV_REL, REL_WHEEL, 1, true);
    ASSERT_EVENT(XIDeviceEvent, up, dpy, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EQ(up->valuators.mask[0], 1 << 2); /* only scroll valuator moved */
    ASSERT_EQ(up->valuators.values[0], 0);
}
#endif

TEST_F(EvdevQEMUTabletTest, HasAbsoluteAxes) {
    XORG_TESTCASE("Create QEMU virtual tablet device.\n"
                  "Ensure abs x and y axes are present.\n");

    ::Display* dpy = Display();

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--", &deviceid), 1);

    int ndevices;
    XIDeviceInfo *info = XIQueryDevice(dpy, deviceid, &ndevices);
    ASSERT_EQ(ndevices, 1);

    bool x_found, y_found = false;
    int naxes = 0;
    for (int i = 0; i < info->num_classes; i++) {
        if (info->classes[i]->type != XIValuatorClass)
            continue;

        naxes++;

        XIValuatorClassInfo *v = reinterpret_cast<XIValuatorClassInfo*>(info->classes[i]);
        if (v->number < 2) {
            ASSERT_EQ(v->mode, Absolute);
            ASSERT_EQ(v->min, 0);
            ASSERT_EQ(v->max, 0x7FFF);

            if (v->number == 0) {
                x_found = true;
                ASSERT_EQ(v->label, XInternAtom(dpy, "Abs X", True));
            } else if (v->number == 1) {
                y_found = true;
                ASSERT_EQ(v->label, XInternAtom(dpy, "Abs Y", True));
            }
        }
    }

    ASSERT_TRUE(x_found);
    ASSERT_TRUE(y_found);
}

TEST_F(EvdevQEMUTabletTest, AbsoluteAxesWork) {
    ::Display* dpy = Display();

    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask);

    int w = DisplayWidth(dpy, DefaultScreen(dpy));
    int h = DisplayHeight(dpy, DefaultScreen(dpy));

    int coords[][4] = {
        { 0x7FFF, 0x7FFF/2, w - 1, (h - 1)/2 },
        { 0x0, 0x0, 0, 0 },
        { 0x7FFF/2, 0x0, (w - 1)/2, 0},
        { 0x7FFF, 0x7FFF/2, w - 1, (h - 1)/2 },
        { 0x7FFF, 0x7FFF, w - 1, h - 1 },
        { -1, -1, -1, -1}
    };

    int i = 0;
    while (coords[i][0] != -1) {
        dev->PlayOne(EV_ABS, ABS_X, coords[i][0]);
        dev->PlayOne(EV_ABS, ABS_Y, coords[i][1], true);
        ASSERT_EVENT(XEvent, ev, dpy, MotionNotify);
        ASSERT_EQ(ev->xmotion.x_root, coords[i][2]);
        ASSERT_EQ(ev->xmotion.y_root, coords[i][3]);
        i++;
    }
}

TEST_F(EvdevQEMUTabletTest, ScrollingWorks) {
    XORG_TESTCASE("Create QEMU virtual tablet device.\n"
                  "Send scroll wheel events.\n"
                  "Verify legacy scroll button events are received\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=54387");

    ::Display* dpy = Display();

    XSelectInput(dpy, DefaultRootWindow(dpy), ButtonPressMask|ButtonReleaseMask);

    dev->PlayOne(EV_REL, REL_WHEEL, -1, true);

    ASSERT_EVENT(XEvent, down_press, dpy, ButtonPress);
    ASSERT_EQ(down_press->xbutton.button, 5U);
    ASSERT_EVENT(XEvent, down_release, dpy, ButtonRelease);
    ASSERT_EQ(down_press->xbutton.button, 5U);

    dev->PlayOne(EV_REL, REL_WHEEL, 1, true);
    ASSERT_EVENT(XEvent, up_press, dpy, ButtonPress);
    ASSERT_EQ(up_press->xbutton.button, 4U);
    ASSERT_EVENT(XEvent, up_release, dpy, ButtonRelease);
    ASSERT_EQ(up_press->xbutton.button, 4U);
}

class EvdevTouchpadTest : public XITServerInputTest,
                          public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device with two wheels.
     */
    virtual void SetUp() {
        SetDevice("touchpads/SynPS2-Synaptics-TouchPad.desc");
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
                               "Option \"GrabDevice\" \"on\""
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "keyboard-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }
};

TEST_F(EvdevTouchpadTest, DeviceExists)
{
    ASSERT_TRUE(FindInputDeviceByName(Display(), "--device--"));
}

TEST_F(EvdevTouchpadTest, AxisLabels)
{
    XORG_TESTCASE("Create evdev touchpad.\n"
                  "Verify axis label property is as expected\n");

    ::Display *dpy = Display();
    const std::string propname = "Axis Labels";

    int deviceid;
    ASSERT_TRUE(FindInputDeviceByName(Display(), "--device--", &deviceid));


    ASSERT_PROPERTY(Atom, labels, dpy, deviceid, propname);
    ASSERT_EQ(labels.nitems, 4U);
    ASSERT_EQ(labels.data[0], XInternAtom(dpy, "Abs X", True));
    ASSERT_EQ(labels.data[1], XInternAtom(dpy, "Abs Y", True));
    ASSERT_EQ(labels.data[2], XInternAtom(dpy, "Abs Pressure", True));
    ASSERT_EQ(labels.data[3], XInternAtom(dpy, "Abs Tool Width", True));
}

TEST_F(EvdevTouchpadTest, PointerMovement)
{
    ::Display *dpy = Display();
    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-move.events");

    XSync(dpy, False);
    ASSERT_GT(XPending(dpy), 0);
    while (XPending(dpy) > 0) {
        ASSERT_EVENT(XEvent, motion, dpy, MotionNotify);
    }
}

class EvdevAbsXMissingTest : public XITServerInputTest,
                      public DeviceInterface
{
public:
    virtual void SetUp() {
        SetDevice("tablets/n4.desc");

        xi2_major_minimum = 2;
        xi2_minor_minimum = 2;

        XITServerInputTest::SetUp();
    }

    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\""
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "keyboard-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }
};

TEST_F(EvdevAbsXMissingTest, DevicePresent)
{
    XORG_TESTCASE("Create device with ABS_MT_POSITION_X but no ABS_X\n"
                  "Driver must not crash\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=64029");

    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--"), 1);

    dev->Play(RECORDINGS_DIR "tablets/n4.events");
}

class EvdevDuplicateTest : public EvdevMouseTest {
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\""
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "keyboard-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.SetAutoAddDevices(true);
        config.WriteConfig();
    }
};

TEST_F(EvdevDuplicateTest, TooManyDevices)
{
    XORG_TESTCASE("Create more than MAXDEVICES evdev devices\n"
                  "Server must not crash\n");

    const int NDEVICES = 50;
    std::auto_ptr<xorg::testing::evemu::Device> devices[NDEVICES];
    for (int i = 0; i < NDEVICES; i++) {
        devices[i] = std::auto_ptr<xorg::testing::evemu::Device>(
                         new xorg::testing::evemu::Device(RECORDINGS_DIR "mice/PIXART-USB-OPTICAL-MOUSE.desc")
                     );
    }

    /* expect at least two hotplugged, but the actual number depends on the
       local number of devices */
    int deviceid;
    ASSERT_GT(FindInputDeviceByName(Display(), "PIXART USB OPTICAL MOUSE", &deviceid), 2);
}

TEST_F(EvdevDuplicateTest, DuplicateDeviceCheck)
{
    XORG_TESTCASE("Add a device through the xorg.conf and hotplugging\n"
                  "Expect it to be added only once\n");

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "PIXART USB OPTICAL MOUSE", &deviceid), 0);
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    std::ifstream logfile(server.GetLogFilePath().c_str());
    std::string line;
    std::string dup_warn("device file is duplicate. Ignoring.");
    bool found = false;
    ASSERT_TRUE(logfile.is_open());
    while(!found && getline(logfile, line))
        found = line.find(dup_warn);

    ASSERT_TRUE(found) << "Expected message '" << dup_warn << "' in the log "
                       << "but couldn't find it";
}

class EvdevDialTest : public EvdevMouseTest {
public:
    virtual void SetUp() {
        SetDevice("mice/Microsoft-Microsoft®-2.4GHz-Transceiver-V2.0.desc");
        XITServerInputTest::SetUp();
    }
};

TEST_F(EvdevDialTest, HorizScrolling)
{
    XORG_TESTCASE("Add a device with REL_DIAL\n"
                  "Send REL_DIAL events\n"
                  "Expect horizontal scroll events\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=73105");

    ::Display *dpy = Display();
    XSelectInput(dpy, DefaultRootWindow(dpy), ButtonPressMask | ButtonReleaseMask);

    dev->PlayOne(EV_REL, REL_DIAL, -1, true);

    ASSERT_EVENT(XEvent, left_press, dpy, ButtonPress);
    ASSERT_EQ(left_press->xbutton.button, 6);
    ASSERT_EVENT(XEvent, left_release, dpy, ButtonRelease);
    ASSERT_EQ(left_release->xbutton.button, 6);

    dev->PlayOne(EV_REL, REL_DIAL, 1, true);

    ASSERT_EVENT(XEvent, right_press, dpy, ButtonPress);
    ASSERT_EQ(right_press->xbutton.button, 7);
    ASSERT_EVENT(XEvent, right_release, dpy, ButtonRelease);
    ASSERT_EQ(right_release->xbutton.button, 7);
}


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
