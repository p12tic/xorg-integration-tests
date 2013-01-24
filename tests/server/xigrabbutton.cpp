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

#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#include "xit-server-input-test.h"
#include "device-interface.h"
#include "helpers.h"


/**
 */
class XIGrabButtonTest : public XITServerInputTest,
                         public DeviceInterface {
public:
    /**
     * Initializes a wacom pad device.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE.desc");
        XITServerInputTest::SetUp();
    }

    /**
     */
    virtual void SetUpConfigAndLog() {

        config.SetAutoAddDevices(false);
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "    Option \"Device\" \"" + dev->GetDeviceNode() + "\""
                               "    Option \"CorePointer\" \"on\"\n"
                               "    Option \"GrabDevice\" \"on\"\n");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "kbd-device",
                               "    Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }
};

static int
grab_buttons (Display *dpy, Window win, int deviceid)
{
    XIGrabModifiers mods;
    unsigned char mask[1] = { 0 };
    XIEventMask evmask;
    int status;

    evmask.deviceid = deviceid;
    evmask.mask_len = sizeof(mask);
    evmask.mask = mask;

    XISetMask (mask, XI_ButtonRelease);
    XISetMask (mask, XI_ButtonPress);

    mods.modifiers = XIAnyModifier;

    status = XIGrabButton (dpy,
                           deviceid,
                           XIAnyButton,
                           win,
                           None,
                           GrabModeAsync,
                           GrabModeAsync,
                           False,
                           &evmask,
                           1,
                           &mods);
    XFlush(dpy);

    return status;
}

static int
setup_event_mask (Display *dpy, Window win, int deviceid)
{
    unsigned char mask[1] = { 0 };
    XIEventMask evmask;
    int status;

    evmask.deviceid = deviceid;
    evmask.mask_len = sizeof(mask);
    evmask.mask = mask;

    XISetMask (evmask.mask, XI_ButtonPress);
    XISetMask (evmask.mask, XI_ButtonRelease);

    status = XISelectEvents (dpy,
                             win,
                             &evmask,
                             1);
    return status;
}

TEST_F(XIGrabButtonTest, GrabWindowTest)
{
    SCOPED_TRACE("\n"
                 "TESTCASE: this test grabs buttons on the root window\n"
                 "and maps a fullscreen override redirect window and expects\n"
                 "events to arrive in the root window and not the regular window");

    ASSERT_GE(RegisterXI2(), 0) << "This test requires XI2" << std::endl;

    ::Display *dpy1 = XOpenDisplay (server.GetDisplayString().c_str());
    ::Display *dpy2 = XOpenDisplay (server.GetDisplayString().c_str());

    XSetWindowAttributes attr;

    attr.background_pixel  = BlackPixel (dpy2, DefaultScreen (dpy2));
    attr.override_redirect = 1;
    attr.event_mask = ButtonPressMask|
                      ButtonReleaseMask|
                      ExposureMask;

    Window win = XCreateWindow(dpy2,
                               DefaultRootWindow(dpy2),
                               0, 0,
                               DisplayWidth(dpy2, DefaultScreen(dpy2)),
                               DisplayHeight(dpy2, DefaultScreen(dpy2)),
                               0,
                               DefaultDepth (dpy2, DefaultScreen (dpy2)),
                               InputOutput,
                               DefaultVisual (dpy2, DefaultScreen (dpy2)),
                               CWBackPixel|CWOverrideRedirect|CWEventMask,
                               &attr);
    XMapRaised (dpy2, win);
    XFlush(dpy2);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy2,
                                                           Expose,
                                                           -1, -1, 1000));

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy2, "--device--", &deviceid), 1);

    // First, check without the grab, the top win should get the event
    XSync(dpy2, False);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy2, ButtonPress, -1, -1));

    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    XEvent ev;
    XNextEvent(dpy2, &ev);

    ASSERT_TRUE(ev.type == ButtonPress);
    XButtonEvent *bev = (XButtonEvent *) &ev;
    ASSERT_TRUE(bev->window == win);

    while (XPending(dpy1))
        XNextEvent (dpy1, &ev);
    ASSERT_FALSE(XPending(dpy1));

    while (XPending(dpy2))
        XNextEvent (dpy2, &ev);
    ASSERT_FALSE(XPending(dpy2));


    // Second, check with XIGrabButton on the root win, the root win should get the event
    setup_event_mask (dpy1, DefaultRootWindow(dpy1), deviceid);
    grab_buttons (dpy1, DefaultRootWindow(dpy1), deviceid);
    XSync(dpy1, False);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy1,
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonPress,
                                                           1000));
    XNextEvent(dpy1, &ev);

    XGenericEventCookie *cookie = &ev.xcookie;

    ASSERT_TRUE(cookie->type == GenericEvent);
    ASSERT_TRUE(cookie->extension == xi2_opcode);
    ASSERT_TRUE(XGetEventData(dpy1, cookie) != 0);

    XIDeviceEvent *xev = (XIDeviceEvent *) cookie->data;
    ASSERT_TRUE(cookie->evtype == XI_ButtonPress);
    ASSERT_TRUE(xev->event == DefaultRootWindow(dpy1));

    XFreeEventData(dpy1, cookie);

    XCloseDisplay(dpy1);
    XCloseDisplay(dpy2);
}

