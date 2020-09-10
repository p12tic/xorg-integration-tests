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
#include <tuple>

#include "helpers.h"
#include "device-interface.h"
#include "xit-server-input-test.h"
#include "xit-event.h"

#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/Xxf86dga.h>

class DGATest : public XITServerInputTest {
public:
    virtual void SetUp() {
        XITServerInputTest::SetUp();

        ASSERT_TRUE(XQueryExtension(Display(), "XFree86-DGA", &dga_opcode, &dga_event_base, &dga_error_base));
        ASSERT_TRUE(XF86DGAQueryExtension(Display(), &dga_event_base, &dga_error_base));

        int major = 2, minor = 0;
        XF86DGAQueryVersion(Display(), &major, &minor);
    }

    int dga_opcode;
    int dga_event_base;
    int dga_error_base;
};

class DGAPointerTest : public DGATest,
                       public DeviceInterface {
public:
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        DGATest::SetUp();
    }

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

TEST_F(DGAPointerTest, StuckPointerButtonDuringGrab)
{
    XORG_TESTCASE("Passively grab a button in GrabModeSync\n"
                  "Press and hold a button\n"
                  "Change DGA mode and set to XF86DGADirectMouse\n"
                  "Select for DGA input\n"
                  "XAllowEvents on the pointer grab\n"
                  "Release pointer\n"
                  "Expect a 0 button mask\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=59100");

    ::Display *dpy = Display();

    XGrabButton(dpy, AnyButton, AnyModifier, DefaultRootWindow(dpy), False,
                    ButtonPressMask | ButtonReleaseMask,
                    GrabModeSync, GrabModeAsync,
                    None, None);

    int nmodes;
    XDGAMode *modes = XDGAQueryModes(dpy, DefaultScreen(dpy), &nmodes);
    ASSERT_GT(nmodes, 0);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);

    ASSERT_EVENT(XButtonEvent, press, dpy, ButtonPress);

    XDGASetMode(dpy, DefaultScreen(dpy), modes[0].num);
    XF86DGADirectVideo(dpy, DefaultScreen(dpy), XF86DGADirectMouse | XF86DGADirectKeyb);
    XDGASelectInput(dpy, DefaultScreen(dpy),
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XAllowEvents(dpy, AsyncPointer, CurrentTime);
    XFlush(dpy);

    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    ASSERT_EVENT(XDGAButtonEvent, dga_release, dpy, dga_event_base + ButtonRelease);

    Window root, child;
    int rx, ry, wx, wy;
    unsigned int mask;
    XQueryPointer(dpy, DefaultRootWindow(dpy), &root, &child,
            &rx, &ry, &wx, &wy, &mask);

    ASSERT_EQ(mask, 0U);

    XFree(modes);
    XDGASelectInput(dpy, DefaultScreen(dpy), 0);
    XF86DGADirectVideo(dpy, DefaultScreen(dpy), 0);
    XDGASetMode(dpy, 0, 0);
    XFlush(dpy);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    ASSERT_EVENT(XButtonEvent, press2, dpy, ButtonPress);
    ASSERT_EVENT(XButtonEvent, release2, dpy, ButtonRelease);
}

