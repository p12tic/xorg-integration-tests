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
#include <stdexcept>
#include <stdint.h>
#include <map>
#include <utility>
#include <sstream>
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
#include "xit-property.h"
#include "xit-event.h"
#include "device-interface.h"
#include "helpers.h"

/**
 * Synaptics driver test for touchpad devices. This class uses a traditional
 * touchpad device.
 */
class SynapticsTest : public XITServerInputTest,
                      public DeviceInterface {
public:
    /**
     * Initializes a standard touchpad device.
     */
    virtual void SetUp() {
        SetDevice("touchpads/SynPS2-Synaptics-TouchPad.desc");

        xi2_major_minimum = 2;
        xi2_minor_minimum = 1;

        XITServerInputTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single synaptics CorePointer device based on
     * the evemu device. Options enabled: tapping (1 finger), two-finger
     * vertical scroll.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("synaptics", "--device--",
                               "Option \"Protocol\"            \"event\"\n"
                               "Option \"CorePointer\"         \"on\"\n"
                               "Option \"TapButton1\"          \"1\"\n"
                               "Option \"GrabEventDevice\"     \"1\"\n"
                               "Option \"FastTaps\"            \"1\"\n"
                               "Option \"VertTwoFingerScroll\" \"1\"\n"
                               "Option \"Device\"              \"" + dev->GetDeviceNode() + "\"\n");
        config.WriteConfig();
    }
};

TEST_F(SynapticsTest, DevicePresent)
{
    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(Display(), XIAllDevices, &ndevices);

    /* VCP, VCK, VC XTEST keyboard, VC XTEST pointer, default keyboard, --device-- */
    ASSERT_EQ(6, ndevices);
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

#ifdef HAVE_XI22
TEST_F(SynapticsTest, SmoothScrollingAvailable)
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
                    ASSERT_EQ(scroll->increment, 100); /* default driver increment */
                    ASSERT_EQ(scroll->flags & XIScrollFlagPreferred, 0);
                    break;
                case XIScrollTypeHorizontal:
                    hscroll_class_found = true;
                    ASSERT_EQ(scroll->increment, 100); /* default driver increment */
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

#ifndef HAVE_RHEL6
/**
 * Synaptics driver test for smooth scrolling increments.
 */
class SynapticsSmoothScrollTest : public SynapticsTest,
                                  public ::testing::WithParamInterface<std::pair<int, int> > {
    /**
     * Sets up an xorg.conf for a single synaptics CorePointer device based on
     * the evemu device. Options enabled: tapping (1 finger), two-finger
     * vertical scroll.
     */
    virtual void SetUpConfigAndLog() {

        std::pair<int, int> params = GetParam();

        std::string vdelta = static_cast<std::stringstream*>( &(std::stringstream() << params.first) )->str();
        std::string hdelta = static_cast<std::stringstream*>( &(std::stringstream() << params.second) )->str();

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("synaptics", "--device--",
                               "Option \"Protocol\"            \"event\"\n"
                               "Option \"CorePointer\"         \"on\"\n"
                               "Option \"TapButton1\"          \"1\"\n"
                               "Option \"GrabEventDevice\"     \"1\"\n"
                               "Option \"FastTaps\"            \"1\"\n"
                               "Option \"VertTwoFingerScroll\" \"1\"\n"
                               "Option \"HorizTwoFingerScroll\" \"1\"\n"
                               "Option \"VertScrollDelta\" \"" + vdelta + "\"\n"
                               "Option \"HorizScrollDelta\" \"" + hdelta + "\"\n"
                               "Option \"Device\"              \"" + dev->GetDeviceNode() + "\"\n");
        config.WriteConfig();
    }

};

TEST_P(SynapticsSmoothScrollTest, ScrollDelta)
{
    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1) << "Failed to find device.";

    int ndevices;
    XIDeviceInfo *info = XIQueryDevice(Display(), deviceid, &ndevices);
    ASSERT_EQ(ndevices, 1);

    std::pair<int, int> deltas = GetParam();

    bool hscroll_class_found = false;
    bool vscroll_class_found = false;
    for (int i = 0; i < info->num_classes; i++) {
        XIAnyClassInfo *any = info->classes[i];
        if (any->type == XIScrollClass) {
            XIScrollClassInfo *scroll = reinterpret_cast<XIScrollClassInfo*>(any);
            switch(scroll->scroll_type) {
                case XIScrollTypeVertical:
                    vscroll_class_found = true;
                    ASSERT_EQ(scroll->increment, deltas.first); /* default driver increment */
                    break;
                case XIScrollTypeHorizontal:
                    hscroll_class_found = true;
                    ASSERT_EQ(scroll->increment, deltas.second); /* default driver increment */
                    break;
                default:
                    FAIL() << "Invalid scroll class: " << scroll->type;
            }
        }
    }

    /* The server doesn't allow a 0 increment on scroll axes, so the device
       comes up with no scroll axes.  */
    if (deltas.second == 0)
        ASSERT_FALSE(hscroll_class_found);
    else
        ASSERT_TRUE(hscroll_class_found);

    if (deltas.first == 0)
        ASSERT_FALSE(vscroll_class_found);
    else
        ASSERT_TRUE(vscroll_class_found);

    XIFreeDeviceInfo(info);
}

INSTANTIATE_TEST_CASE_P(, SynapticsSmoothScrollTest,
                        ::testing::Values(std::make_pair(0, 0),
                                          std::make_pair(1, 1),
                                          std::make_pair(100, 100),
                                          std::make_pair(-1, -1),
                                          std::make_pair(-100, 1),
                                          std::make_pair(1, 100)
                            ));

#endif
#endif /* HAVE_XI22 */


void check_buttons_event(::Display *display,
                        xorg::testing::evemu::Device *dev,
                        const std::string& events_path,
                        unsigned int button,
                        int expect_nevents) {

    dev->Play(RECORDINGS_DIR "touchpads/" + events_path);
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

    /* if we get more than 5 events difference, something is probably wrong */
    ASSERT_LT(abs(expect_nevents - nevents), 5);
}

TEST_F(SynapticsTest, ScrollWheel)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    /* the server takes a while to start up bust the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());
    check_buttons_event(Display(), dev.get(), "SynPS2-Synaptics-TouchPad-two-finger-scroll-up.events", 4, 17);
    check_buttons_event(Display(), dev.get(), "SynPS2-Synaptics-TouchPad-two-finger-scroll-down.events", 5, 10);
}

TEST_F(SynapticsTest, TapEvent)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    /* the server takes a while to start up bust the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());
    check_buttons_event(Display(), dev.get(), "SynPS2-Synaptics-TouchPad-tap.events", 1, 1);
}

void check_drag_event(::Display *display,
                      xorg::testing::evemu::Device *dev,
                      const std::string& events_path,
                      int expect_npress,
                      int expect_nmotion,
                      int expect_nrelease) {

    dev->Play(RECORDINGS_DIR "touchpads/" + events_path);
    XSync(display, False);

    ASSERT_NE(XPending(display), 0) << "No event pending" << std::endl;
    XEvent ev;
    int nevents = 0;

    while (XCheckMaskEvent (display, ButtonPressMask, &ev))
        nevents++;
    ASSERT_EQ(expect_npress, nevents);
    ASSERT_EQ(ev.xbutton.button, 1U);

    nevents = 0;
    while (XCheckMaskEvent (display, PointerMotionMask, &ev))
        nevents++;
    /* if we get more than 5 events difference, something is probably wrong */
    ASSERT_LT(abs(expect_nmotion -nevents), 5);

    nevents = 0;
    while (XCheckMaskEvent (display, ButtonReleaseMask, &ev))
        nevents++;
    ASSERT_EQ(expect_nrelease, nevents);
    ASSERT_EQ(ev.xbutton.button, 1U);

    while(XPending(display))
        XNextEvent(display, &ev);
}

TEST_F(SynapticsTest, TapAndDragEvent)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    /* the server takes a while to start up bust the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());
    /* 1 press, 30 moves then 1 release is what we expect */
    check_drag_event(Display(), dev.get(), "SynPS2-Synaptics-TouchPad-tap-and-move.events", 1, 30, 1);
}

/**
 * Synaptics driver test for Bug 53037 - Device coordinate scaling breaks
 * XWarpPointer requests
 *
 * If a device with a device coordinate range is the lastSlave, a
 * WarpPointer request on some coordinates may end up on different pixels
 * than requested. The server scales from screen to device coordinates, then
 * back to screen - a rounding error may then change to the wrong pixel.
 *
 * https://bugs.freedesktop.org/show_bug.cgi?id=53037
 *
 * Takes a pair of integers, not used by the class itself.
 */
class SynapticsWarpTest : public SynapticsTest,
                          public ::testing::WithParamInterface<std::pair<int, int> >{
};

TEST_P(SynapticsWarpTest, WarpScaling)
{
    XORG_TESTCASE("https://bugs.freedesktop.org/show_bug.cgi?id=53037");

    /* Play one event so this device is lastSlave */
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-move.events");

    int x = GetParam().first,
        y = GetParam().second;

    XWarpPointer(Display(), 0, DefaultRootWindow(Display()), 0, 0, 0, 0, x, y);

    Window root, child;
    int rx, ry, wx, wy;
    unsigned int mask;

    XQueryPointer(Display(), DefaultRootWindow(Display()), &root, &child,
                  &rx, &ry, &wx, &wy, &mask);
    ASSERT_EQ(root, DefaultRootWindow(Display()));
    /* Warping to negative should clip to 0 */
    ASSERT_EQ(rx, std::max(x, 0));
    ASSERT_EQ(ry, std::max(y, 0));
}

INSTANTIATE_TEST_CASE_P(, SynapticsWarpTest,
                        ::testing::Values(std::make_pair(0, 0),
                                          std::make_pair(1, 1),
                                          std::make_pair(2, 2),
                                          std::make_pair(200, 200),
                                          std::make_pair(-1, -1)
                            ));


class SynapticsWarpZaphodTest : public SynapticsWarpTest {
    virtual void SetUpConfigAndLog() {
        if (xinerama.empty())
            xinerama = "off";
        config.SetAutoAddDevices(true); /* to avoid ServerLayout being
                                           written */
        config.AppendRawConfig(
            "Section \"ServerLayout\"\n"
            "	Identifier     \"X.org Configured\"\n"
            "	Screen         0 \"Screen0\"\n"
            "	Screen         1 \"Screen1\" LeftOf \"Screen0\"\n"
            "	Option         \"Xinerama\" \"" + xinerama + "\n"
            "	Option         \"AutoAddDevices\" \"off\"\n"
            "EndSection\n"
            "\n"
            "Section \"Device\"\n"
            "	Identifier  \"Card0\"\n"
            "	Driver      \"dummy\"\n"
            "EndSection\n"
            "\n"
            "Section \"Device\"\n"
            "	Identifier  \"Card1\"\n"
            "	Driver      \"dummy\"\n"
            "EndSection\n"
            "\n"
            "Section \"Screen\"\n"
            "	Identifier \"Screen0\"\n"
            "	Device     \"Card0\"\n"
            "EndSection\n"
            "\n"
            "Section \"Screen\"\n"
            "	Identifier \"Screen1\"\n"
            "	Device     \"Card1\"\n"
            "EndSection"
            "\n"
            "Section \"InputDevice\"\n"
            "   Identifier \"synaptics\"\n"
            "   Driver \"synaptics\"\n"
            "   Option \"Device\" \"" + dev->GetDeviceNode() + "\"\n"
            "EndSection\n");
        config.WriteConfig();
    }

protected:
    std::string xinerama;
};

TEST_P(SynapticsWarpZaphodTest, WarpScaling)
{
    XORG_TESTCASE("Start server with Xinerama off\n"
                  "Play synaptics event to ensure it is the lastSlave\n"
                  "Warp pointer on left root window\n"
                  "Compare coordinates to expected\n"
                  "Warp pointer on right root window\n"
                  "Compare coordinates to expected\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=53037");

    ::Display *dpy = Display();
    ASSERT_EQ(ScreenCount(dpy), 2);

    /* Play one event so this device is lastSlave */
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-move.events");

    int x = GetParam().first,
        y = GetParam().second;

    Window lroot = RootWindow(dpy, 1);
    /* Warp on left root window */
    XWarpPointer(dpy, 0, lroot, 0, 0, 0, 0, x, y);

    Window root, child;
    int rx, ry, wx, wy;
    unsigned int mask;

    bool same_screen;
    same_screen= XQueryPointer(dpy, lroot, &root, &child,
                               &rx, &ry, &wx, &wy, &mask);
    ASSERT_TRUE(same_screen);
    ASSERT_EQ(root, lroot);

    /* Warping to negative of left screen should clip to 0 */
    ASSERT_EQ(rx, std::max(x, 0));
    ASSERT_EQ(ry, std::max(y, 0));

    /* Now warp to right root window */
    Window rroot = RootWindow(dpy, 0);
    XWarpPointer(dpy, 0, rroot, 0, 0, 0, 0, x, y);
    /* Query from left root window */
    same_screen = XQueryPointer(dpy, lroot, &root, &child,
                               &rx, &ry, &wx, &wy, &mask);
    ASSERT_FALSE(same_screen);
    ASSERT_EQ(root, rroot);
    ASSERT_EQ(rx, std::max(x, 0));
    ASSERT_EQ(ry, std::max(y, 0));

    /* WarpPointer doesn't let us warp relative between two screens, so we
       can't test that here */
}

INSTANTIATE_TEST_CASE_P(, SynapticsWarpZaphodTest,
                        ::testing::Values(std::make_pair(0, 0),
                                          std::make_pair(1, 1),
                                          std::make_pair(2, 2),
                                          std::make_pair(200, 200),
                                          std::make_pair(-1, -1)
                            ));

class SynapticsWarpXineramaTest : public SynapticsWarpZaphodTest {
    virtual void SetUp() {
        xinerama = "on";
        SynapticsWarpZaphodTest::SetUp();
    }
};

TEST_P(SynapticsWarpXineramaTest, WarpScaling)
{
    XORG_TESTCASE("Start server with Xinerama on\n"
                  "Play synaptics event to ensure it is the lastSlave\n"
                  "Warp pointer on left root window\n"
                  "Compare coordinates to expected\n"
                  "Warp pointer on right root window\n"
                  "Compare coordinates to expected\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=53037");

    ::Display *dpy = Display();
    ASSERT_EQ(ScreenCount(dpy), 1);

    /* Play one event so this device is lastSlave */
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-move.events");

    int x = GetParam().first,
        y = GetParam().second;

    Window root = DefaultRootWindow(dpy);

    /* Warp on left screen */
    XWarpPointer(dpy, 0, root, 0, 0, 0, 0, x, y);

    Window r, child;
    int rx, ry, wx, wy;
    unsigned int mask;

    XQueryPointer(dpy, root, &r, &child,
                  &rx, &ry, &wx, &wy, &mask);

    /* Warping to negative of left screen should clip to 0 */
    ASSERT_EQ(rx, std::max(x, 0));
    ASSERT_EQ(ry, std::max(y, 0));

    int w = DisplayWidth(dpy, 0);

    /* Now warp to right screen */
    XWarpPointer(dpy, 0, root, 0, 0, 0, 0, w/2 + x, y);
    XQueryPointer(dpy, root, &root, &child,
                 &rx, &ry, &wx, &wy, &mask);
    ASSERT_EQ(rx, w/2 + x);
    ASSERT_EQ(ry, std::max(y, 0));
}

TEST_F(SynapticsWarpXineramaTest, WarpScalingRelative)
{
    XORG_TESTCASE("Start server with Xinerama on\n"
                  "Play synaptics event to ensure it is the lastSlave\n"
                  "Warp pointer to right screen\n"
                  "Warp pointer relatively to the left\n"
                  "Compare coordinates to expected\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=53037");

    ::Display *dpy = Display();
    ASSERT_EQ(ScreenCount(dpy), 1);

    /* Play one event so this device is lastSlave */
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-move.events");

    Window root = DefaultRootWindow(dpy);

    int w = DisplayWidth(dpy, 0);

    /* Warp to right screen */
    XWarpPointer(dpy, 0, root, 0, 0, 0, 0, w/2 + 10, 100);

    Window child;
    int rx, ry, wx, wy;
    unsigned int mask;

    /* Now warp relative right to left window */
    XWarpPointer(dpy, 0, None, 0, 0, 0, 0, -100, 0);
    XQueryPointer(dpy, root, &root, &child,
                 &rx, &ry, &wx, &wy, &mask);
    ASSERT_EQ(rx, w/2 + 10 - 100);
    ASSERT_EQ(ry, 100);
}

INSTANTIATE_TEST_CASE_P(, SynapticsWarpXineramaTest,
                        ::testing::Values(std::make_pair(0, 0),
                                          std::make_pair(1, 1),
                                          std::make_pair(2, 2),
                                          std::make_pair(200, 200),
                                          std::make_pair(-1, -1)
                            ));

/**
 * Synaptics driver test for clickpad devices.
 */
class SynapticsClickpadTest : public XITServerInputTest,
                              public DeviceInterface {
public:
    /**
     * Initializes a clickpad, as the one found on the Lenovo x220t.
     */
    virtual void SetUp() {
        SetDevice("touchpads/SynPS2-Synaptics-TouchPad-Clickpad.desc");
        XITServerInputTest::SetUp();
        nfingers_down = 0;
    }

    /**
     * Set up a single clickpad CorePointer device.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("synaptics", "--device--",
                               "Option \"CorePointer\"         \"on\"\n"
                               "Option \"TapButton1\"          \"1\"\n"
                               "Option \"GrabEventDevice\"     \"1\"\n"
                               "Option \"VertTwoFingerScroll\" \"1\"\n"
                               "Option \"Device\"              \"" + dev->GetDeviceNode() + "\"\n");
        config.WriteConfig();
    }

    int nfingers_down;

    virtual void TouchBegin(unsigned int slot, int x, int y) {
        static unsigned int tracking_id;
        nfingers_down |= 1 << slot;
        dev->PlayOne(EV_KEY, BTN_TOUCH, 1);
        dev->PlayOne(EV_ABS, ABS_MT_SLOT, slot);
        dev->PlayOne(EV_ABS, ABS_PRESSURE, 40);
        dev->PlayOne(EV_ABS, ABS_MT_TRACKING_ID, tracking_id++);
        TouchUpdate(slot, x, y);
    }

    virtual void TouchUpdate(unsigned int slot, int x, int y, bool sync = true) {
        dev->PlayOne(EV_ABS, ABS_MT_SLOT, slot);
        dev->PlayOne(EV_ABS, ABS_X, x);
        dev->PlayOne(EV_ABS, ABS_Y, y);
        dev->PlayOne(EV_ABS, ABS_MT_POSITION_X, x);
        dev->PlayOne(EV_ABS, ABS_MT_POSITION_Y, y);
        dev->PlayOne(EV_ABS, ABS_MT_PRESSURE, 68);
        if (sync)
            dev->PlayOne(EV_SYN, SYN_REPORT, 0);
    }

    virtual void TouchEnd(unsigned int slot) {
        dev->PlayOne(EV_ABS, ABS_MT_SLOT, slot);
        dev->PlayOne(EV_ABS, ABS_MT_TRACKING_ID, -1);
        nfingers_down &= ~(1 << slot);
        if (!nfingers_down)
            dev->PlayOne(EV_KEY, BTN_TOUCH, 0);
        dev->PlayOne(EV_SYN, SYN_REPORT, 0);
    }

    virtual void
    SetClickfingerProperty(const char* name,
                           unsigned char cf1 = 1, unsigned char cf2 = 1, unsigned char cf3 = 1)
    {
        ::Display *dpy = Display();
        int deviceid;
        ASSERT_EQ(FindInputDeviceByName(dpy, name, &deviceid), 1);

        Atom clickfinger_prop = XInternAtom(dpy, "Synaptics Click Action", True);
        ASSERT_NE(clickfinger_prop, (Atom)None);

        unsigned char data[3] = {cf1, cf2, cf3};

        XIChangeProperty(dpy, deviceid, clickfinger_prop, XA_INTEGER, 8,
                         PropModeReplace, data, 3);
        XSync(dpy, False);
    }



};

static void
set_clickpad_property(Display *dpy, const char* name)
{
    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, name, &deviceid), 1);
    ASSERT_PROPERTY(char, clickpad_prop, dpy, deviceid, "Synaptics ClickPad");
    clickpad_prop.data[0] = 1;
    clickpad_prop.Update();
    XSync(dpy, False);
}

TEST_F(SynapticsClickpadTest, ClickpadProperties)
{
#ifndef INPUT_PROP_BUTTONPAD
    SCOPED_TRACE("<linux/input.h>'s INPUT_PROP_BUTTONPAD is missing.\n"
                 "Clickpad detection will not work on this kernel");
#endif
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    ASSERT_PROPERTY(char, prop_clickpad, Display(), deviceid, "Synaptics ClickPad");
    ASSERT_EQ(prop_clickpad.type, XA_INTEGER);
    ASSERT_EQ(prop_clickpad.format, 8);
    ASSERT_EQ(prop_clickpad.nitems, 1);
#ifdef INPUT_PROP_BUTTONPAD
    ASSERT_EQ(prop_clickpad.data[0], 1);
#else
    ASSERT_EQ(prop_clickpad.data[0], 0) << "Not expecting ClickPad to be set on a kernel without INPUT_PROP_BUTTONPAD";
#endif

    /* This option is assigned by the xorg.conf.d, it won't activate for
       xorg.conf devices. */
#ifndef INPUT_PROP_BUTTONPAD
    int prop = XInternAtom(Display(), "Synaptics Soft Button Areas", True);
    ASSERT_EQ(prop, (Atom)None) << "Not expecting Soft Buttons property on non-clickpads";
    return;
#endif

    ASSERT_PROPERTY(int, prop_softbutton, Display(), deviceid, "Synaptics Soft Button Areas");

    ASSERT_EQ(prop_softbutton.format, 32);
    ASSERT_EQ(prop_softbutton.nitems, 8);
    ASSERT_EQ(prop_softbutton.type, XA_INTEGER);

    ASSERT_EQ(prop_softbutton.data[0], 0);
    ASSERT_EQ(prop_softbutton.data[1], 0);
    ASSERT_EQ(prop_softbutton.data[2], 0);
    ASSERT_EQ(prop_softbutton.data[3], 0);
    ASSERT_EQ(prop_softbutton.data[4], 0);
    ASSERT_EQ(prop_softbutton.data[5], 0);
    ASSERT_EQ(prop_softbutton.data[6], 0);
    ASSERT_EQ(prop_softbutton.data[7], 0);
}

TEST_F(SynapticsClickpadTest, Tap)
{
    set_clickpad_property(Display(), "--device--");
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.tap.events");

    XSync(Display(), False);

    XEvent btn;
    ASSERT_NE(XPending(Display()), 0) << "No event pending" << std::endl;
    XNextEvent(Display(), &btn);

    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 1U);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, 1U);

    XSync(Display(), True);
}

TEST_F(SynapticsClickpadTest, VertScrollDown)
{
    set_clickpad_property(Display(), "--device--");
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.two-finger-scroll-down.events");

    XSync(Display(), False);

    XEvent btn;
    ASSERT_NE(XPending(Display()), 0) << "No event pending" << std::endl;
    XNextEvent(Display(), &btn);

    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 5U);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, 5U);

    XSync(Display(), True);
}

TEST_F(SynapticsClickpadTest, VertScrollUp)
{
    set_clickpad_property(Display(), "--device--");
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.two-finger-scroll-up.events");

    XSync(Display(), False);

    XEvent btn;
    ASSERT_NE(XPending(Display()), 0) << "No event pending" << std::endl;
    XNextEvent(Display(), &btn);

    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 4U);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, 4U);

    XSync(Display(), True);
}

TEST_F(SynapticsClickpadTest, DisableDevice)
{
    XORG_TESTCASE("Disable and re-enable the device with no fingers down\n");

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    DeviceSetEnabled(Display(), deviceid, false);
    DeviceSetEnabled(Display(), deviceid, true);
}

TEST_F(SynapticsClickpadTest, DisableDeviceOneFingerDownAndLift)
{
    XORG_TESTCASE("Disable the device with one fingers down, lift finger,\n"
                  "re-enable device.");

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.one-finger-down.events");
    DeviceSetEnabled(Display(), deviceid, false);
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.one-finger-up.events");
    DeviceSetEnabled(Display(), deviceid, true);
}

TEST_F(SynapticsClickpadTest, DisableDeviceOneFingerDown)
{
    XORG_TESTCASE("Disable the device with one finger down, re-enable with\n"
                  "finger still down\n");

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.one-finger-down.events");
    DeviceSetEnabled(Display(), deviceid, false);
    DeviceSetEnabled(Display(), deviceid, true);
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.one-finger-up.events");
}

TEST_F(SynapticsClickpadTest, DisableDeviceTwoFingersDownAndLift)
{
    XORG_TESTCASE("Disable the device with two fingers down, lift fingers, "
                  "re-enable.");
    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.two-finger-down.events");
    DeviceSetEnabled(Display(), deviceid, false);
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.two-finger-up.events");
    DeviceSetEnabled(Display(), deviceid, true);
}

TEST_F(SynapticsClickpadTest, DisableDeviceTwoFingersDown)
{
    XORG_TESTCASE("Disable the device with two fingers down, re-enable, with\n"
                  "fingers still down");

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.two-finger-down.events");
    DeviceSetEnabled(Display(), deviceid, false);
    DeviceSetEnabled(Display(), deviceid, true);
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.two-finger-up.events");
}

TEST_F(SynapticsClickpadTest, DisableDeviceOneFingerResume)
{
    XORG_TESTCASE("Disable the device with no fingers down, re-enable with\n"
                  "one finger down\n");

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    DeviceSetEnabled(Display(), deviceid, false);
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.one-finger-down.events");
    DeviceSetEnabled(Display(), deviceid, true);
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.one-finger-up.events");
}

TEST_F(SynapticsClickpadTest, DisableDeviceTwoFingersResume)
{
    XORG_TESTCASE("Disable the device with no fingers down, re-enable with\n"
                  "two fingers down");

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    DeviceSetEnabled(Display(), deviceid, false);
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.two-finger-down.events");
    DeviceSetEnabled(Display(), deviceid, true);
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.two-finger-up.events");
}

TEST_F(SynapticsClickpadTest, DisableDeviceOneFingerTwoFingersResume)
{
    XORG_TESTCASE("Disable the device with one finger down, re-enable with\n"
                  "two fingers down");

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.one-finger-down.events");
    DeviceSetEnabled(Display(), deviceid, false);
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.one-finger-up.events");
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.two-finger-down.events");
    DeviceSetEnabled(Display(), deviceid, true);
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.two-finger-up.events");
}

TEST_F(SynapticsClickpadTest, DisableDeviceTwoFingersOneFingerResume)
{
    XORG_TESTCASE("Disable the device with two fingers down, re-enable with\n"
                  "one finger down");

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.two-finger-down.events");
    DeviceSetEnabled(Display(), deviceid, false);
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.two-finger-up.events");
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.one-finger-down.events");
    DeviceSetEnabled(Display(), deviceid, true);
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.one-finger-up.events");
}

TEST_F(SynapticsClickpadTest, ClickFinger2)
{
    XORG_TESTCASE("Put two fingers on the touchpad\n"
                  "Trigger BTN_LEFT\n"
                  "Expect ClickFinger2 action\n");
    ::Display *dpy = Display();

    SetClickfingerProperty("--device--", 0, 2, 0);

    XSelectInput(dpy, DefaultRootWindow(dpy), ButtonPressMask | ButtonReleaseMask);

    TouchBegin(0, 2000, 2000);
    TouchBegin(1, 2300, 2010);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    TouchEnd(1);
    TouchEnd(0);

    ASSERT_EVENT(XEvent, press, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, release, dpy, ButtonRelease);
    ASSERT_EQ(press->xbutton.button, 2);
    ASSERT_EQ(release->xbutton.button, 2);
}

TEST_F(SynapticsClickpadTest, ClickFinger2Distance)
{
    XORG_TESTCASE("Put two fingers on the touchpad, too far apart for clickfinger\n"
                  "Trigger BTN_LEFT\n"
                  "Expect ClickFinger1 action\n");
    ::Display *dpy = Display();

    SetClickfingerProperty("--device--", 3, 2, 0);

    XSelectInput(dpy, DefaultRootWindow(dpy), ButtonPressMask | ButtonReleaseMask);

    TouchBegin(0, 2000, 2000);
    TouchBegin(1, 4300, 2010);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    TouchEnd(1);
    TouchEnd(0);

    /* one finger on a clickpad doesn't trigger clickfinger, just normal
       button press */
    ASSERT_EVENT(XEvent, press, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, release, dpy, ButtonRelease);
    ASSERT_EQ(press->xbutton.button, 1);
    ASSERT_EQ(release->xbutton.button, 1);
}

TEST_F(SynapticsClickpadTest, ClickFinger3)
{
    XORG_TESTCASE("Put two fingers on the touchpad\n"
                  "Trigger BTN_TOOL_TRIPLETAP\n"
                  "Trigger BTN_LEFT\n"
                  "Expect ClickFinger2 action\n");
    ::Display *dpy = Display();

    SetClickfingerProperty("--device--", 0, 0, 2);

    XSelectInput(dpy, DefaultRootWindow(dpy), ButtonPressMask | ButtonReleaseMask);

    TouchBegin(0, 2000, 2000);
    TouchBegin(1, 2300, 2010);
    /* this touchpad only does two touchpoints */
    dev->PlayOne(EV_KEY, BTN_TOOL_TRIPLETAP, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    TouchEnd(1);
    TouchEnd(0);

    ASSERT_EVENT(XEvent, press, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, release, dpy, ButtonRelease);
    ASSERT_EQ(press->xbutton.button, 2);
    ASSERT_EQ(release->xbutton.button, 2);
}

TEST_F(SynapticsClickpadTest, ClickFinger3Distance)
{
    XORG_TESTCASE("Put two fingers on the touchpad, far apart\n"
                  "Trigger BTN_TOOL_TRIPLETAP\n"
                  "Trigger BTN_LEFT\n"
                  "Expect ClickFinger3 action\n");
    ::Display *dpy = Display();

    SetClickfingerProperty("--device--", 0, 2, 3);

    XSelectInput(dpy, DefaultRootWindow(dpy), ButtonPressMask | ButtonReleaseMask);

    TouchBegin(0, 2000, 2000);
    TouchBegin(1, 4300, 2010);
    dev->PlayOne(EV_KEY, BTN_TOOL_TRIPLETAP, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    TouchEnd(1);
    TouchEnd(0);

    /* xf86-input-synaptics-1.7.99.1-7-ga6f0f4c changed the behaviour, now
       we always expect clickfinger 3, regardless of distance */
    ASSERT_EVENT(XEvent, press, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, release, dpy, ButtonRelease);
    ASSERT_EQ(press->xbutton.button, 3);
    ASSERT_EQ(release->xbutton.button, 3);
}

TEST_F(SynapticsClickpadTest, SecondaryButtonPropertyNotSet)
{
    XORG_TESTCASE("Secondary software button property must not exist on this device\n");

    int deviceid;
    ASSERT_TRUE(FindInputDeviceByName(Display(), "--device--", &deviceid));
    ASSERT_FALSE(DevicePropertyExists(Display(), deviceid, "Synaptics Secondary Soft Button Areas"));
}

/**
 * Synaptics driver test for clickpad devices with the SoftButtonArea option
 * set.
 */
class SynapticsClickpadSoftButtonsTest : public SynapticsClickpadTest {
public:
    /**
     * Sets up a single CorePointer synaptics clickpad device with the
     * SoftButtonArea option set to 50% left/right, 82% from the top.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("synaptics", "--device--",
                               "Option \"CorePointer\"         \"on\"\n"
                               "Option \"ClickPad\"            \"on\"\n"
                               "Option \"GrabEventDevice\"     \"1\"\n"
                               "Option \"SoftButtonAreas\"     \"50% 0 82% 0 0 0 0 0\"\n"
                               "Option \"Device\"              \"" + dev->GetDeviceNode() + "\"\n");
        config.WriteConfig();
    }
};

TEST_F(SynapticsClickpadSoftButtonsTest, LeftClick)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.left-phys-click.events");

    XEvent btn;
    ASSERT_NE(XPending(Display()), 0) << "No event pending" << std::endl;
    XNextEvent(Display(), &btn);

    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 1U);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, 1U);

    XSync(Display(), True);
}

TEST_F(SynapticsClickpadSoftButtonsTest, RightClick)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.right-phys-click.events");

    XEvent btn;
    ASSERT_NE(XPending(Display()), 0) << "No event pending" << std::endl;
    XNextEvent(Display(), &btn);

    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 3U);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, 3U);

    XSync(Display(), True);
}

enum {
    RBL = 0,
    RBR = 1,
    RBT = 2,
    RBB = 3,
    MBL = 4,
    MBR = 5,
    MBT = 6,
    MBB = 7,
};

TEST_F(SynapticsClickpadSoftButtonsTest, InvalidButtonArea)
{
    XORG_TESTCASE("Update soft button area property with ranges of \n"
                  "left > right or top > bottom\n");

    ::Display *dpy = Display();

    int deviceid;
    ASSERT_TRUE(FindInputDeviceByName(dpy, "--device--", &deviceid));

    XITProperty<int> softbutton_prop(dpy, deviceid, "Synaptics Soft Button Areas");

    softbutton_prop.data[RBL] = 3000;
    softbutton_prop.data[RBR] = 2000;

    SetErrorTrap(dpy);
    softbutton_prop.Update();
    ASSERT_ERROR(ReleaseErrorTrap(dpy), BadValue);

    softbutton_prop.data[RBT] = 3000;
    softbutton_prop.data[RBB] = 2000;

    SetErrorTrap(dpy);
    softbutton_prop.Update();
    XSync(dpy, True);
    ASSERT_ERROR(ReleaseErrorTrap(dpy), BadValue);

    softbutton_prop.data[MBL] = 3000;
    softbutton_prop.data[MBR] = 2000;

    SetErrorTrap(dpy);
    softbutton_prop.Update();
    ASSERT_ERROR(ReleaseErrorTrap(dpy), BadValue);

    softbutton_prop.data[MBT] = 3000;
    softbutton_prop.data[MBB] = 2000;

    SetErrorTrap(dpy);
    softbutton_prop.Update();
    ASSERT_ERROR(ReleaseErrorTrap(dpy), BadValue);
}

TEST_F(SynapticsClickpadSoftButtonsTest, SinglePixelOverlap)
{
    XORG_TESTCASE("Overlap of soft button areas by one 1 device unit is permitted");

    ::Display *dpy = Display();

    int deviceid;
    ASSERT_TRUE(FindInputDeviceByName(dpy, "--device--", &deviceid));

    XITProperty<int> softbutton_prop(dpy, deviceid, "Synaptics Soft Button Areas");

    softbutton_prop.data[RBL] = 2000;
    softbutton_prop.data[RBR] = 3000;
    softbutton_prop.data[MBL] = 1000;
    softbutton_prop.data[MBR] = 2000;

    softbutton_prop.data[RBT] = 2000;
    softbutton_prop.data[MBT] = 2000;
    softbutton_prop.data[RBB] = 3000;
    softbutton_prop.data[MBB] = 3000;

    SetErrorTrap(dpy);
    softbutton_prop.Update();
    ASSERT_TRUE(ReleaseErrorTrap(dpy) == NULL);

    softbutton_prop.data[RBL] = 2000;
    softbutton_prop.data[RBR] = 3000;
    softbutton_prop.data[MBL] = 2000;
    softbutton_prop.data[MBR] = 3000;

    softbutton_prop.data[RBT] = 2000;
    softbutton_prop.data[MBT] = 3000;
    softbutton_prop.data[RBB] = 3000;
    softbutton_prop.data[MBB] = 4000;

    SetErrorTrap(dpy);
    softbutton_prop.Update();
    ASSERT_TRUE(ReleaseErrorTrap(dpy) == NULL);
}

class SynapticsClickpadSoftButtonsWithAreaTest : public SynapticsClickpadSoftButtonsTest {
public:
    /**
     * Sets up a single CorePointer synaptics clickpad device with the
     * SoftButtonArea option set to 50% left/right, 82% from the top.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("synaptics", "--device--",
                               "Option \"CorePointer\"         \"on\"\n"
                               "Option \"ClickPad\"            \"on\"\n"
                               "Option \"GrabEventDevice\"     \"1\"\n"
                               "Option \"SoftButtonAreas\"     \"50% 0 82% 0 0 0 0 0\"\n"
                               "Option \"AreaBottomEdge\"     \"3900\"\n"
                               "Option \"Device\"              \"" + dev->GetDeviceNode() + "\"\n");
        config.WriteConfig();
    }
};

TEST_F(SynapticsClickpadSoftButtonsTest, LeftClickInDeadArea)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.left-phys-click.events");

    XEvent btn;
    ASSERT_NE(XPending(Display()), 0) << "No event pending" << std::endl;
    XNextEvent(Display(), &btn);

    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 1U);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, 1U);

    XSync(Display(), True);
}

TEST_F(SynapticsClickpadSoftButtonsWithAreaTest, RightClickInDeadArea)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.right-phys-click.events");

    XEvent btn;
    ASSERT_NE(XPending(Display()), 0) << "No event pending" << std::endl;
    XNextEvent(Display(), &btn);

    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 3U);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, 3U);

    XSync(Display(), True);
}

/**
 * Synaptics driver test for clickpad devices with the SoftButtonArea set at
 * runtime.
 */
class SynapticsClickpadSoftButtonsRuntimeTest : public SynapticsClickpadTest {
public:
    /**
     * Sets up a single CorePointer synaptics clickpad device with the
     * SoftButtonArea option set to 50% left/right, 82% from the top.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("synaptics", "--device--",
                               "Option \"CorePointer\"         \"on\"\n"
                               "Option \"ClickPad\"            \"off\"\n"
                               "Option \"GrabEventDevice\"     \"1\"\n"
                               "Option \"Device\"              \"" + dev->GetDeviceNode() + "\"\n");
        config.WriteConfig();
    }
};

TEST_F(SynapticsClickpadSoftButtonsRuntimeTest, SoftButtonsFirst)
{
    XORG_TESTCASE("https://bugs.freedesktop.org/show_bug.cgi?id=54102");

    ::Display *dpy = Display();
    Atom softbutton_prop = XInternAtom(dpy, "Synaptics Soft Button Areas", True);
    ASSERT_EQ(softbutton_prop, (Atom)None);

    softbutton_prop = XInternAtom(dpy, "Synaptics Soft Button Areas", False);

    int deviceid;
    ASSERT_TRUE(FindInputDeviceByName(dpy, "--device--", &deviceid));

    uint32_t propdata[8] = {0};
    propdata[0] = 3472;
    propdata[2] = 3900;

    XIChangeProperty(dpy, deviceid, softbutton_prop, XA_INTEGER, 32,
                     PropModeReplace,
                     reinterpret_cast<unsigned char*>(propdata), 8);
    XSync(dpy, False);

    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.right-phys-click.events");

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(), ButtonPress, -1, -1, 1000));

    /* expect left-click, the clickpad prop is still off */
    XEvent btn;
    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 1U);

    Atom clickpad_prop = XInternAtom(dpy, "Synaptics ClickPad", False);
    unsigned char on = 1;
    XIChangeProperty(dpy, deviceid, clickpad_prop, XA_INTEGER, 8,
                     PropModeReplace, &on, 1);
    XSync(Display(), False);

    /* now expect right click */
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.right-phys-click.events");

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(), ButtonPress, -1, -1, 1000));

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 3U);
}

TEST_F(SynapticsClickpadSoftButtonsRuntimeTest, SoftButtonsSecond)
{
    ::Display *dpy = Display();
    Atom softbutton_prop = XInternAtom(dpy, "Synaptics Soft Button Areas", True);
    ASSERT_EQ(softbutton_prop, (Atom)None);


    int deviceid;
    ASSERT_TRUE(FindInputDeviceByName(dpy, "--device--", &deviceid));


    Atom clickpad_prop = XInternAtom(dpy, "Synaptics ClickPad", False);
    unsigned char on = 1;
    XIChangeProperty(dpy, deviceid, clickpad_prop, XA_INTEGER, 8,
                     PropModeReplace, &on, 1);
    XSync(Display(), False);

    /* prop must now be initialized */
    softbutton_prop = XInternAtom(dpy, "Synaptics Soft Button Areas", True);
    ASSERT_GT(softbutton_prop, (Atom)None);

    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.right-phys-click.events");

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(), ButtonPress, -1, -1, 1000));

    /* expect left-click, soft button areas should be 0 */
    XEvent btn;
    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 1U);

    uint32_t propdata[8] = {0};
    propdata[0] = 3472;
    propdata[2] = 3900;

    XIChangeProperty(dpy, deviceid, softbutton_prop, XA_INTEGER, 32,
                     PropModeReplace,
                     reinterpret_cast<unsigned char*>(propdata), 8);
    XSync(dpy, False);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.right-phys-click.events");

    /* now expect right click */
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(), ButtonPress, -1, -1, 1000));

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 3U);
}

TEST(SynapticsClickPad, HotPlugSoftButtons)
{
#ifndef INPUT_PROP_BUTTONPAD
    SCOPED_TRACE("<linux/input.h>'s INPUT_PROP_BUTTONPAD is missing.\n"
                 "Clickpad detection will not work on this kernel");
#endif
    std::auto_ptr<xorg::testing::evemu::Device> dev;

    dev = std::auto_ptr<xorg::testing::evemu::Device>(
            new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.desc"
                )
            );

    XOrgConfig config;
    XITServer server;

    config.SetAutoAddDevices(true);
    config.AddDefaultScreenWithDriver();
    server.Start(config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    int deviceid;
    int ndevices = FindInputDeviceByName(dpy, "SynPS/2 Synaptics TouchPad", &deviceid);
    ASSERT_GT(ndevices, 0);
    EXPECT_EQ(ndevices, 1) << "More than one touchpad found, cannot "
                              "guarantee right behaviour.";

    ASSERT_PROPERTY(char, prop_clickpad, dpy, deviceid, "Synaptics ClickPad");
    ASSERT_EQ(prop_clickpad.type, XA_INTEGER);
    ASSERT_EQ(prop_clickpad.format, 8);
    ASSERT_EQ(prop_clickpad.nitems, 1);

#ifdef INPUT_PROP_BUTTONPAD
    ASSERT_EQ(prop_clickpad.data[0], 1U);
#else
    ASSERT_EQ(prop_clickpad.data[0], 0U) << "Not expecting ClickPad to be set on a kernel without INPUT_PROP_BUTTONPAD";
#endif

    /* This option is assigned by the xorg.conf.d, it won't activate for
       xorg.conf devices. */
#ifndef INPUT_PROP_BUTTONPAD
    int prop = XInternAtom(dpy, "Synaptics Soft Button Areas", True);
    ASSERT_EQ(prop, (Atom)None) << "Not expecting Soft Buttons property on non-clickpads";
    return;
#endif

    ASSERT_PROPERTY(int, prop_softbutton, dpy, deviceid, "Synaptics Soft Button Areas");

    ASSERT_EQ(prop_softbutton.format, 32);
    ASSERT_EQ(prop_softbutton.nitems, 8);
    ASSERT_EQ(prop_softbutton.type, XA_INTEGER);

    ASSERT_EQ(prop_softbutton.data[0], 3472);
    ASSERT_EQ(prop_softbutton.data[1], 0);
    ASSERT_EQ(prop_softbutton.data[2], 3900);
    ASSERT_EQ(prop_softbutton.data[3], 0);
    ASSERT_EQ(prop_softbutton.data[4], 0);
    ASSERT_EQ(prop_softbutton.data[5], 0);
    ASSERT_EQ(prop_softbutton.data[6], 0);
    ASSERT_EQ(prop_softbutton.data[7], 0);

    config.RemoveConfig();
    server.RemoveLogFile();
}

class SynapticsScrollButtonTest : public SynapticsTest {
public:
    virtual void SetUp() {
        SetDevice("touchpads/SynPS2-Synaptics-TouchPad-ScrollButtons.events");
        XITServerInputTest::SetUp();
    }

    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("synaptics", "--device--",
                               "Option \"CorePointer\"         \"on\"\n"
                               "Option \"GrabEventDevice\"     \"1\"\n"
                               "Option \"Device\"              \"" + dev->GetDeviceNode() + "\"\n");
        config.WriteConfig();
    }

};

TEST_F(SynapticsScrollButtonTest, ScrollButtonPropertyExists)
{
    XORG_TESTCASE("Initialize with a touchpad with scroll buttons\n"
                  "Make sure the scroll button properties exists\n");

    ::Display *dpy = Display();
    int deviceid;
    ASSERT_TRUE(FindInputDeviceByName(dpy, "--device--", &deviceid));

    XITProperty<char> scrollbutton_prop(dpy, deviceid, "Synaptics Button Scrolling");
    XITProperty<char> scrollbutton_repeat_prop(dpy, deviceid, "Synaptics Button Scrolling Repeat");
    XITProperty<int> scrollbutton_time_prop(dpy, deviceid, "Synaptics Button Scrolling Time");
}

TEST_F(SynapticsScrollButtonTest, ScrollButtonUpDownScroll)
{
    XORG_TESTCASE("Initialize with a touchpad with scroll buttons\n"
                  "Trigger events for BTN_0 and BTN_1\n"
                  "Expect up/down scrolling, respectively\n");

    ::Display *dpy = Display();
    int deviceid;
    ASSERT_TRUE(FindInputDeviceByName(dpy, "--device--", &deviceid));

    XSelectInput(dpy, DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);

    dev->PlayOne(EV_KEY, BTN_0, 1, true);
    dev->PlayOne(EV_KEY, BTN_0, 0, true);
    dev->PlayOne(EV_KEY, BTN_0, 1, true);
    dev->PlayOne(EV_KEY, BTN_0, 0, true);

    ASSERT_EVENT(XEvent, up1, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, up2, dpy, ButtonRelease);
    ASSERT_EVENT(XEvent, up3, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, up4, dpy, ButtonRelease);

    ASSERT_EQ(up1->xbutton.button, 4);
    ASSERT_EQ(up2->xbutton.button, 4);
    ASSERT_EQ(up3->xbutton.button, 4);
    ASSERT_EQ(up4->xbutton.button, 4);

    dev->PlayOne(EV_KEY, BTN_1, 1, true);
    dev->PlayOne(EV_KEY, BTN_1, 0, true);
    dev->PlayOne(EV_KEY, BTN_1, 1, true);
    dev->PlayOne(EV_KEY, BTN_1, 0, true);

    ASSERT_EVENT(XEvent, down1, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, down2, dpy, ButtonRelease);
    ASSERT_EVENT(XEvent, down3, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, down4, dpy, ButtonRelease);

    ASSERT_EQ(down1->xbutton.button, 5);
    ASSERT_EQ(down2->xbutton.button, 5);
    ASSERT_EQ(down3->xbutton.button, 5);
    ASSERT_EQ(down4->xbutton.button, 5);
}

TEST_F(SynapticsScrollButtonTest, ScrollButtonUpDownMiddleDouble)
{
    XORG_TESTCASE("Initialize with a touchpad with scroll buttons\n"
                  "Disable up/down scrolling through property\n"
                  "Trigger events for BTN_0 and BTN_1\n"
                  "Expect middle and double-click, respectively\n");

    ::Display *dpy = Display();
    int deviceid;
    ASSERT_TRUE(FindInputDeviceByName(dpy, "--device--", &deviceid));

    XITProperty<char> scrollbutton_prop(dpy, deviceid, "Synaptics Button Scrolling");
    scrollbutton_prop.data[0] = 0;
    scrollbutton_prop.Update();

    XSelectInput(dpy, DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);

    dev->PlayOne(EV_KEY, BTN_0, 1, true);
    dev->PlayOne(EV_KEY, BTN_0, 0, true);

    ASSERT_EVENT(XEvent, double1, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, double2, dpy, ButtonRelease);
    ASSERT_EVENT(XEvent, double3, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, double4, dpy, ButtonRelease);

    ASSERT_EQ(double1->xbutton.button, 1);
    ASSERT_EQ(double2->xbutton.button, 1);
    ASSERT_EQ(double3->xbutton.button, 1);
    ASSERT_EQ(double4->xbutton.button, 1);

    dev->PlayOne(EV_KEY, BTN_1, 1, true);
    dev->PlayOne(EV_KEY, BTN_1, 0, true);
    dev->PlayOne(EV_KEY, BTN_1, 1, true);
    dev->PlayOne(EV_KEY, BTN_1, 0, true);

    ASSERT_EVENT(XEvent, middle1, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, middle2, dpy, ButtonRelease);
    ASSERT_EVENT(XEvent, middle3, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, middle4, dpy, ButtonRelease);

    ASSERT_EQ(middle1->xbutton.button, 2);
    ASSERT_EQ(middle2->xbutton.button, 2);
    ASSERT_EQ(middle3->xbutton.button, 2);
    ASSERT_EQ(middle4->xbutton.button, 2);
}

class SynapticsSecondarySoftButtonTest : public SynapticsClickpadTest {
public:
    /**
     * Initializes a standard touchpad device.
     */
    virtual void SetUp() {
        SetDevice("touchpads/SynPS2-Synaptics-TouchPad-T440.desc");

        xi2_major_minimum = 2;
        xi2_minor_minimum = 1;

        XITServerInputTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single synaptics CorePointer device based on
     * the evemu device. Options enabled: tapping (1 finger), two-finger
     * vertical scroll.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("synaptics", "--device--",
                               "Option \"Protocol\"            \"event\"\n"
                               "Option \"CorePointer\"         \"on\"\n"
                               "Option \"TapButton1\"          \"1\"\n"
                               "Option \"GrabEventDevice\"     \"1\"\n"
                               "Option \"FastTaps\"            \"1\"\n"
                               "Option \"VertTwoFingerScroll\" \"1\"\n"
                               "Option \"SecondarySoftButtonAreas\" \"58% 0 0 8% 42% 58% 0 8%\"\n"
                               "Option \"Device\"              \"" + dev->GetDeviceNode() + "\"\n");
        config.WriteConfig();
    }
};

TEST_F(SynapticsSecondarySoftButtonTest, PropertyExists)
{
    XORG_TESTCASE("Start with a default value for SecondarySoftButtonAreas\n"
                  "Expect the property to exist and be non-zero");

    ::Display *dpy = Display();
    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--", &deviceid), 1);

    ASSERT_PROPERTY(int, prop, dpy, deviceid, "Synaptics Secondary Soft Button Areas");

    ASSERT_EQ(prop.type, XA_INTEGER);
    ASSERT_EQ(prop.nitems, 8);
    ASSERT_EQ(prop.format, 32);

    ASSERT_NE(prop.data[0], 0);
    ASSERT_NE(prop.data[3], 0);
    ASSERT_NE(prop.data[4], 0);
    ASSERT_NE(prop.data[5], 0);
    ASSERT_NE(prop.data[7], 0);

    ASSERT_EQ(prop.data[1], 0);
    ASSERT_EQ(prop.data[2], 0);
    ASSERT_EQ(prop.data[6], 0);
}

TEST_F(SynapticsSecondarySoftButtonTest, NoMovementWithinTopButtons)
{
    XORG_TESTCASE("Start with the top buttons enabled\n"
                  "Put a finger down in the top buttons\n"
                  "Move finger across, staying in the area\n"
                  "Don't expect events\n");

    ::Display *dpy = Display();

    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask | ButtonPressMask | ButtonReleaseMask);

    TouchBegin(0, 1500, 1500);
    TouchUpdate(0, 1600, 1500);
    TouchUpdate(0, 1700, 1500);
    TouchEnd(0);

    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_F(SynapticsSecondarySoftButtonTest, NoMovementInTopButtonsAfterClick)
{
    XORG_TESTCASE("Start with the top buttons enabled\n"
                  "Put a finger down in the top buttons\n"
                  "Press\n"
                  "Move finger out of button area\n"
                  "Don't expect motion events\n");

    ::Display *dpy = Display();

    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask | ButtonPressMask | ButtonReleaseMask);

    TouchBegin(0, 1500, 1500);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    TouchUpdate(0, 1600, 1500);
    TouchUpdate(0, 1700, 1500);

    ASSERT_EVENT(XEvent, press, dpy, ButtonPress);
    ASSERT_TRUE(NoEventPending(dpy));

    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    TouchEnd(0);
    ASSERT_EVENT(XEvent, release, dpy, ButtonRelease);
}

TEST_F(SynapticsSecondarySoftButtonTest, MovementInTopButtonsMovingOut)
{
    XORG_TESTCASE("Start with the top buttons enabled\n"
                  "Put a finger down in the top buttons\n"
                  "Move finger out of the buttons\n"
                  "Expect motion events\n");

    ::Display *dpy = Display();

    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask | ButtonPressMask | ButtonReleaseMask);

    TouchBegin(0, 1500, 1500);
    TouchUpdate(0, 1600, 2600);
    TouchUpdate(0, 1700, 3500);
    TouchEnd(0);

    ASSERT_EVENT(XEvent, motion, dpy, MotionNotify);
}

TEST_F(SynapticsSecondarySoftButtonTest, MovementIntoTopButtons)
{
    XORG_TESTCASE("Start with the top buttons enabled\n"
                  "Put a finger down outside the top buttons\n"
                  "Move finger into the buttons\n"
                  "Expect motion events\n"
                  "Move finger within the buttons\n"
                  "Expect motion events\n");

    ::Display *dpy = Display();

    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask | ButtonPressMask | ButtonReleaseMask);

    TouchBegin(0, 1500, 3500);
    TouchUpdate(0, 1500, 2500);
    TouchUpdate(0, 1500, 1500);

    while (XPending(dpy)) {
        ASSERT_EVENT(XEvent, motion, dpy, MotionNotify);
        XSync(dpy, False);
    }

    TouchUpdate(0, 1600, 1500);
    TouchUpdate(0, 1700, 1500);
    TouchUpdate(0, 1800, 1500);
    TouchUpdate(0, 1900, 1500);

    TouchEnd(0);

    ASSERT_EVENT(XEvent, motion, dpy, MotionNotify);
    while (XPending(dpy)) {
        ASSERT_EVENT(XEvent, motion, dpy, MotionNotify);
        XSync(dpy, False);
    }
}

TEST_F(SynapticsSecondarySoftButtonTest, TopButtonLeftClick)
{
    XORG_TESTCASE("Start with the top buttons set\n"
                  "Put a finger down in the top buttons\n"
                  "Click\n"
                  "Expect left click\n");

    ::Display *dpy = Display();

    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask | ButtonPressMask | ButtonReleaseMask);

    TouchBegin(0, 1500, 1500);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    TouchEnd(0);

    ASSERT_EVENT(XEvent, press, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, release, dpy, ButtonRelease);
    ASSERT_EQ(press->xbutton.button, 1);
    ASSERT_EQ(release->xbutton.button, 1);
}

TEST_F(SynapticsSecondarySoftButtonTest, TopButtonRightClick)
{
    XORG_TESTCASE("Start with the top buttons set\n"
                  "Put a finger down in the top buttons\n"
                  "Click\n"
                  "Expect right click\n");

    ::Display *dpy = Display();

    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask | ButtonPressMask | ButtonReleaseMask);

    TouchBegin(0, 4500, 1500);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    TouchEnd(0);

    ASSERT_EVENT(XEvent, press, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, release, dpy, ButtonRelease);
    ASSERT_EQ(press->xbutton.button, 3);
    ASSERT_EQ(release->xbutton.button, 3);
}

TEST_F(SynapticsSecondarySoftButtonTest, TopButtonMiddleClick)
{
    XORG_TESTCASE("Start with the top buttons set\n"
                  "Put a finger down in the top buttons\n"
                  "Click\n"
                  "Expect middle click\n");

    ::Display *dpy = Display();

    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask | ButtonPressMask | ButtonReleaseMask);

    TouchBegin(0, 3292, 1500);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    TouchEnd(0);

    ASSERT_EVENT(XEvent, press, dpy, ButtonPress);
    ASSERT_EVENT(XEvent, release, dpy, ButtonRelease);
    ASSERT_EQ(press->xbutton.button, 2);
    ASSERT_EQ(release->xbutton.button, 2);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
