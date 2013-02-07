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

#include <X11/extensions/XInput2.h>

#include "xit-server-input-test.h"
#include "xit-event.h"
#include "device-interface.h"
#include "helpers.h"

using namespace xorg::testing;

class PointerMotionTest : public XITServerInputTest,
                          public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        XITServerInputTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "kbd-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }
};

class PointerButtonMotionMaskTest : public PointerMotionTest,
                                    public ::testing::WithParamInterface<int>
{
};

TEST_P(PointerButtonMotionMaskTest, ButtonXMotionMask)
{
    XORG_TESTCASE("Select for ButtonXMotionMask\n"
                  "Move pointer\n"
                  "Verify no events received\n"
                  "Press button and move\n"
                  "Verify events received\n");
    int button = GetParam();

    ::Display *dpy = Display();
    XSelectInput(dpy, DefaultRootWindow(dpy), Button1MotionMask << (button - 1));
    XSync(dpy, False);

    dev->PlayOne(EV_REL, REL_X, 10, true);
    XSync(dpy, False);
    ASSERT_EQ(XPending(dpy), 0);

    int devbutton;
    switch(button) {
        case 1: devbutton = BTN_LEFT; break;
        case 2: devbutton = BTN_MIDDLE; break;
        case 3: devbutton = BTN_RIGHT; break;
        default: FAIL();
    }

    dev->PlayOne(EV_KEY, devbutton, 1, true);
    dev->PlayOne(EV_REL, REL_X, 10, true);
    dev->PlayOne(EV_KEY, devbutton, 0, true);

    ASSERT_EVENT(XEvent, motion, dpy, MotionNotify);
    ASSERT_TRUE(motion->xmotion.state & (Button1Mask << (button -1)));

    XSync(dpy, False);
    ASSERT_EQ(XPending(dpy), 0);
}

INSTANTIATE_TEST_CASE_P(, PointerButtonMotionMaskTest, ::testing::Range(1, 4));

class PointerSubpixelTest : public PointerMotionTest {
    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n"
                               "Option \"ConstantDeceleration\" \"20\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "kbd-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }
};

TEST_F(PointerSubpixelTest, NoSubpixelCoreEvents)
{
    XORG_TESTCASE("Move pointer by less than a pixels\n"
                  "Ensure no core motion event is received\n"
                  "Ensure XI2 motion events are received\n");

    ::Display *dpy = Display();
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy2);

    double x, y;
    QueryPointerPosition(dpy, &x, &y);

    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask);
    XSync(dpy, False);

    dev->PlayOne(EV_REL, REL_X, 1, true);
    dev->PlayOne(EV_REL, REL_X, 1, true);
    dev->PlayOne(EV_REL, REL_X, 1, true);
    dev->PlayOne(EV_REL, REL_X, 1, true);

    ASSERT_FALSE(XServer::WaitForEvent(Display(), 500));

    XSelectInput(dpy, DefaultRootWindow(dpy), NoEventMask);
    XSync(dpy, False);

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_Motion);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_Motion);

    XISelectEvents(dpy2, DefaultRootWindow(dpy), &mask, 1);
    XSync(dpy2, False);

    delete[] mask.mask;

    dev->PlayOne(EV_REL, REL_X, 1, true);
    dev->PlayOne(EV_REL, REL_X, 1, true);
    dev->PlayOne(EV_REL, REL_X, 1, true);
    dev->PlayOne(EV_REL, REL_X, 1, true);

    ASSERT_TRUE(XServer::WaitForEventOfType(dpy2, GenericEvent, xi2_opcode, XI_Motion));

    double new_x, new_y;
    QueryPointerPosition(dpy, &new_x, &new_y);
    ASSERT_EQ(x, new_x);
    ASSERT_EQ(y, new_y);

    XCloseDisplay(dpy2);
}

