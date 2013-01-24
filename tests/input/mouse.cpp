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

/**
 * Mouse driver test.
 */
class MouseTest : public XITServerInputTest,
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
     * Sets up an xorg.conf for a single mouse CorePointer device based on
     * the evemu device.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("mouse", "--device--",
                               "Option \"ZAxisMapping\" \"4 5 6 7\"\n"
                               "Option \"Protocol\" \"ImPS/2\"\n"
                               "Option \"CorePointer\" \"on\"\n");
        config.WriteConfig();
    }
};

void scroll_wheel_event(::Display *display,
                        xorg::testing::evemu::Device *dev,
                        int axis, int value, unsigned int button) {

    dev->PlayOne(EV_REL, axis, value, 1);
    XSync(display, False);

    ASSERT_EQ(xorg::testing::XServer::WaitForEventOfType(display, ButtonPress, -1, -1, 1000), true);

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


TEST_F(MouseTest, ScrollWheel)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XFlush(Display());
    XSync(Display(), False);

    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, 1, 4);
    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, 2, 4);
    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, 3, 4);

    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, -1, 5);
    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, -2, 5);
    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, -3, 5);

    /* Vertical scrolling only, it appears we can't send HScroll events */
}

TEST_F(MouseTest, Move)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), PointerMotionMask);
    XFlush(Display());
    XSync(Display(), False);

    dev->PlayOne(EV_REL, REL_X, -1, 1);

    ASSERT_EQ(xorg::testing::XServer::WaitForEventOfType(Display(), MotionNotify, -1, -1, 1000), true);

    int x, y;
    XEvent ev;
    XNextEvent(Display(), &ev);
    x = ev.xmotion.x_root;
    y = ev.xmotion.y_root;

    /* left */
    dev->PlayOne(EV_REL, REL_X, -1, 1);
    ASSERT_EQ(xorg::testing::XServer::WaitForEventOfType(Display(), MotionNotify, -1, -1, 1000), true);

    XNextEvent(Display(), &ev);
    ASSERT_LT(ev.xmotion.x_root, x);
    ASSERT_EQ(ev.xmotion.y_root, y);
    x = ev.xmotion.x_root;

    /* right */
    dev->PlayOne(EV_REL, REL_X, 1, 1);
    ASSERT_EQ(xorg::testing::XServer::WaitForEventOfType(Display(), MotionNotify, -1, -1, 1000), true);

    XNextEvent(Display(), &ev);
    ASSERT_GT(ev.xmotion.x_root, x);
    ASSERT_EQ(ev.xmotion.y_root, y);
    x = ev.xmotion.x_root;

    /* up */
    dev->PlayOne(EV_REL, REL_Y, -1, 1);
    ASSERT_EQ(xorg::testing::XServer::WaitForEventOfType(Display(), MotionNotify, -1, -1, 1000), true);

    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.xmotion.x_root, x);
    ASSERT_LT(ev.xmotion.y_root, y);
    y = ev.xmotion.y_root;

    /* down */
    dev->PlayOne(EV_REL, REL_Y, 1, 1);
    ASSERT_EQ(xorg::testing::XServer::WaitForEventOfType(Display(), MotionNotify, -1, -1, 1000), true);

    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.xmotion.x_root, x);
    ASSERT_GT(ev.xmotion.y_root, y);
    y = ev.xmotion.y_root;
}

TEST_F(MouseTest, BtnPress)
{
    XORG_TESTCASE("Terminating the server with a button down crashes the server.\n"
                  "http://patchwork.freedesktop.org/patch/12193/");

    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, 1);

    ASSERT_EQ(xorg::testing::XServer::WaitForEventOfType(Display(), ButtonPress, -1, -1, 1000), true);
    XEvent ev;
    XNextEvent(Display(), &ev);
    ASSERT_FALSE(XPending(Display()));
}

TEST_F(MouseTest, BtnRelease)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonReleaseMask);
    XSync(Display(), False);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);

    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    ASSERT_EQ(xorg::testing::XServer::WaitForEventOfType(Display(), ButtonRelease, -1, -1, 1000), true);
    XEvent ev;
    XNextEvent(Display(), &ev);

    ASSERT_FALSE(XPending(Display()));
}

TEST_F(MouseTest, DevNode)
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
    ASSERT_EQ(devnode.compare("/dev/input/mice"), 0);

    XFree(data);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
