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

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <limits.h>

#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

#include "xorg-conf.h"
#include "xit-server.h"
#include "xit-event.h"
#include "helpers.h"
#include "device-interface.h"

using namespace xorg::testing;

class MultiheadTest : public Test,
                      private ::testing::EmptyTestEventListener {
public:
    virtual void WriteConfig(bool left_of, bool xinerama) {
        config_path = server.GetConfigPath();
        std::ofstream config(config_path.c_str());
        config << ""
            "Section \"ServerLayout\"\n"
            "	Identifier     \"X.org Configured\"\n"
            "	Screen         0 \"Screen0\"\n"
            "	Screen         1 \"Screen1\" " << ((left_of) ? "LeftOf" : "RightOf") << " \"Screen0\"\n"
            "	Option         \"Xinerama\" \"" << (xinerama ? "on" : "off") << "\"\n"
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
            "EndSection";
        config.close();
    }

    virtual void SetUp() {
        WriteConfig(left_of, xinerama);

        server.SetDisplayNumber(133);
        server.Start();

        failed = false;

        testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
        listeners.Append(this);
    }

    virtual void TearDown() {
        if (!failed) {
            unlink(config_path.c_str());
            server.RemoveLogFile();
        }

        testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
        listeners.Release(this);
    }

    void OnTestPartResult(const ::testing::TestPartResult &test_part_result) {
        failed = test_part_result.failed();
    }

protected:
    XITServer server;
    bool xinerama;
    bool left_of;

    std::string config_path;
private:
    std::string log_path;
    bool failed;
};

class ZaphodTest : public MultiheadTest,
                   public DeviceInterface,
                   public ::testing::WithParamInterface<bool> {
public:
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE.desc");
        xinerama = false;
        left_of = GetParam();
        MultiheadTest::SetUp();
    }

    void VerifyLeaveEvent(::Display *dpy) {
        XSync(dpy, False);

        XEvent ev;
        XNextEvent(dpy, &ev);
        ASSERT_EQ(ev.type, LeaveNotify);
        ASSERT_FALSE(ev.xcrossing.same_screen);
        ASSERT_EQ(ev.xcrossing.mode, NotifyNormal);
        ASSERT_EQ(ev.xcrossing.detail, NotifyNonlinear);
        /* no more events must be in the pipe */
        if (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            FAIL() <<
                "After the LeaveNotify, no more events are expected on this screen.\n" <<
                "Event in the pipe is of type " << ev.type;
        }
    }

    void VerifyEnterEvent(::Display *dpy) {
        XEvent ev;
        XSync(dpy, False);
        ASSERT_TRUE(XPending(dpy));
        XNextEvent(dpy, &ev);
        ASSERT_EQ(ev.type, EnterNotify);
        ASSERT_EQ(ev.xcrossing.mode, NotifyNormal);
        ASSERT_EQ(ev.xcrossing.detail, NotifyNonlinear);
    }

    void MoveUntilMotionEvent(::Display *dpy, int direction)
    {
        while (!XPending(dpy)) {
            dev->PlayOne(EV_REL, REL_X, direction * 3, true);
            XSync(dpy, False);
        }
    }

    void FlushMotionEvents(::Display *dpy, int direction)
    {
        dev->PlayOne(EV_REL, REL_X, direction * 3, true);
        XSync(dpy, False);
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            ASSERT_EQ(ev.type, MotionNotify);
        }
    }

    /**
     * Move into the given x direction until the pointer has either hit the
     * edge of the screen, or a non-motion event is in the queue.
     *
     * @return false If the next even in the queue is not a motion event,
     * true if we hit the edge of the screen
     */
    bool Move(::Display *dpy, int direction)
    {
        XEvent ev;
        bool on_screen = true;
        int old_x = (direction > 0) ? INT_MIN : INT_MAX;
        do {
            MoveUntilMotionEvent(dpy, direction);
            bool done = false;
            while (XPending(dpy) && !done) {
                XNextEvent(dpy, &ev);

                switch(ev.type) {
                    case LeaveNotify:
                        on_screen = false;
                        XPutBackEvent(dpy, &ev);
                        done = true;
                        break;
                    case MotionNotify:
                        if (direction < 0)
                            EXPECT_LT(ev.xmotion.x_root, old_x) <<
                                "Pointer must move in negative X direction only";
                        else
                            EXPECT_GT(ev.xmotion.x_root, old_x) <<
                                "Pointer must move in positive X direction only";
                        old_x = ev.xmotion.x_root;
                        break;
                    default:
                        ADD_FAILURE() << "Unknown event type " << ev.type;
                        done = true;
                        break;
                }
            }
        } while(on_screen &&
                old_x < (DisplayWidth(dpy, 0) - 1) &&
                old_x != 0 && ev.xmotion.x_root != 0);

        return on_screen;
    }
};

TEST_P(ZaphodTest, ScreenCrossingXTestAbsolute)
{
    XORG_TESTCASE("In a setup with two ScreenRec (xinerama off), \n"
                  "moving the pointer through XTest causes the pointer to move to the requested screen.\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=60001");

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy);
    ASSERT_EQ(ScreenCount(dpy), 2);

    /* we keep two separate connections to make the code a bit simpler */
    ::Display *dpy2 = XOpenDisplay(std::string(server.GetDisplayString() + ".1").c_str());
    ASSERT_TRUE(dpy2);
    ASSERT_EQ(ScreenCount(dpy2), 2);

    XSelectInput(dpy, RootWindow(dpy, 0), PointerMotionMask | LeaveWindowMask | EnterWindowMask);
    XSelectInput(dpy2, RootWindow(dpy2, 1), PointerMotionMask | EnterWindowMask | LeaveWindowMask);
    XSync(dpy, False);
    XSync(dpy2, False);

    /* Move to screen 0 */
    XTestFakeMotionEvent(dpy, 0, 10, 10, 0);
    XSync(dpy, False);
    ASSERT_EVENT(XEvent, s1_motion, dpy, MotionNotify);
    /* now to the other screen */
    XTestFakeMotionEvent(dpy, 1, 10, 10, 0);
    XSync(dpy, False);

    ASSERT_EVENT(XEvent, s1_leave, dpy, LeaveNotify);
    XSync(dpy, False);

    ASSERT_EVENT(XEvent, s2_enter, dpy2, EnterNotify);
    ASSERT_EVENT(XEvent, s2_enter_motion, dpy2, MotionNotify);
}

TEST_P(ZaphodTest, ScreenCrossing)
{
    XORG_TESTCASE("In a setup with two ScreenRec (xinerama off), \n"
                  "moving the pointer causes the pointer to switch screens.\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=54654");

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy);
    ASSERT_EQ(ScreenCount(dpy), 2);

    /* we keep two separate connections to make the code a bit simpler */
    ::Display *dpy2 = XOpenDisplay(std::string(server.GetDisplayString() + ".1").c_str());
    ASSERT_TRUE(dpy2);
    ASSERT_EQ(ScreenCount(dpy2), 2);

    XSelectInput(dpy, RootWindow(dpy, 0), PointerMotionMask | LeaveWindowMask | EnterWindowMask);
    XSelectInput(dpy2, RootWindow(dpy2, 1), PointerMotionMask | EnterWindowMask | LeaveWindowMask);
    XSync(dpy, False);
    XSync(dpy2, False);

    bool left_of = GetParam();
    int direction = left_of ? -1 : 1;
    bool on_screen;

    int start_x;
    if (left_of)
        start_x = 1;
    else
        start_x = DisplayWidth(dpy, DefaultScreen(dpy)) - 2;

    WarpPointer(dpy, start_x, DisplayHeight(dpy, DefaultScreen(dpy))/2);

    on_screen = Move(dpy, direction);

    /* We've left the screen, make sure we get an enter on the other one */
    ASSERT_FALSE(on_screen) << "Expected pointer to leave screen";
    VerifyLeaveEvent(dpy);
    VerifyEnterEvent(dpy2);

    /* move the pointer in further so we don't cross back on the first
       event, and check for spurious events on the second screen */
    FlushMotionEvents(dpy2, direction);

    ASSERT_FALSE(XPending(dpy)) << "No events should appear on first screen";
    ASSERT_FALSE(XPending(dpy2));

    if (left_of)
        start_x = DisplayWidth(dpy2, DefaultScreen(dpy2)) - 2;
    else
        start_x = 1;

    WarpPointer(dpy2, start_x, DisplayHeight(dpy2, DefaultScreen(dpy2))/2);

    /* We're on the second screen now, move back */
    direction *= -1;
    on_screen = Move(dpy2, direction);

    ASSERT_FALSE(on_screen) << "Expected pointer to leave screen";
    VerifyLeaveEvent(dpy2);
    on_screen = false;

    VerifyEnterEvent(dpy);
    FlushMotionEvents(dpy, direction);
}

TEST_P(ZaphodTest, ScreenCrossingButtons)
{
    XORG_TESTCASE("Start a zaphod server with two screens."
                  "Create a window on the left and one on the right edge.\n"
                  "Move pointer to the left window, click.\n"
                  "Expect enter notify, button events on left window.\n"
                  "Move pointer to the right window, click.\n"
                  "Expect leave notify on left window, enter notify, button events on left window.\n");

    std::string ldisplay;
    std::string rdisplay;

    bool left_of = GetParam();
    if (left_of) {
        rdisplay = server.GetDisplayString();
        ldisplay = server.GetDisplayString() + ".1";
    } else {
        ldisplay = server.GetDisplayString();
        rdisplay = server.GetDisplayString() + ".1";
    }

    ::Display *ldpy = XOpenDisplay(ldisplay.c_str());
    ASSERT_TRUE(ldpy);
    ASSERT_EQ(ScreenCount(ldpy), 2);
    XSynchronize(ldpy, True);

    /* we keep two separate connections to make the code a bit simpler */
    ::Display *rdpy = XOpenDisplay(rdisplay.c_str());
    ASSERT_TRUE(rdpy);
    ASSERT_EQ(ScreenCount(rdpy), 2);
    XSynchronize(rdpy, True);

    int width = DisplayWidth(ldpy, DefaultScreen(ldpy));
    int height = DisplayHeight(ldpy, DefaultScreen(ldpy));

    Window lwin = CreateWindow(ldpy, None, 0, 0, 200, height);
    Window rwin = CreateWindow(rdpy, None, width - 200, 0, 200, height);

    XSelectInput(ldpy, lwin, EnterWindowMask|LeaveWindowMask|ButtonPressMask|ButtonReleaseMask);
    XSelectInput(rdpy, rwin, EnterWindowMask|LeaveWindowMask|ButtonPressMask|ButtonReleaseMask);

    dev->PlayOne(EV_REL, REL_X, -width * 2, true);

    ASSERT_EVENT(XEnterWindowEvent, lwin_enter, ldpy, EnterNotify);
    ASSERT_EQ(lwin_enter->window, lwin);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    ASSERT_EVENT(XButtonPressedEvent, lwin_press, ldpy, ButtonPress);
    ASSERT_EQ(lwin_enter->window, lwin);
    ASSERT_EVENT(XButtonPressedEvent, lwin_release, ldpy, ButtonRelease);
    ASSERT_EQ(lwin_enter->window, lwin);

    dev->PlayOne(EV_REL, REL_X, width * 2, true);

    ASSERT_EVENT(XLeaveWindowEvent, lwin_leave, ldpy, LeaveNotify);
    ASSERT_EQ(lwin_enter->window, lwin);

    ASSERT_EVENT(XEnterWindowEvent, rwin_enter, rdpy, EnterNotify);
    ASSERT_EQ(rwin_enter->window, rwin);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    ASSERT_EVENT(XButtonPressedEvent, rwin_press, rdpy, ButtonPress);
    ASSERT_EQ(rwin_enter->window, rwin);
    ASSERT_EVENT(XButtonPressedEvent, rwin_release, rdpy, ButtonRelease);
    ASSERT_EQ(rwin_enter->window, rwin);
}

INSTANTIATE_TEST_CASE_P(, ZaphodTest, ::testing::Values(true, false));

class XineramaTest : public ZaphodTest {
public:
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE.desc");
        xinerama = true;
        left_of = GetParam();
        MultiheadTest::SetUp();
    }
};

TEST_P(XineramaTest, ScreenCrossing)
{
    XORG_TESTCASE("In a Xinerama setup, move left right, hitting "
                  "the screen boundary each time");
    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy);
    ASSERT_EQ(ScreenCount(dpy), 1);

    XSelectInput(dpy, RootWindow(dpy, 0), PointerMotionMask | LeaveWindowMask | EnterWindowMask);
    XFlush(dpy);

    int width = DisplayWidth(dpy, DefaultScreen(dpy));
    double x, y;
    bool left_of = GetParam();
    int direction = left_of ? -1 : 1;
    bool on_screen = Move(dpy, direction);
    ASSERT_TRUE(on_screen);
    QueryPointerPosition(dpy, &x, &y);
    ASSERT_EQ(x, (direction == -1) ? 0 : width - 1);

    direction *= -1;
    on_screen = Move(dpy, direction);
    ASSERT_TRUE(on_screen);
    QueryPointerPosition(dpy, &x, &y);
    ASSERT_EQ(x, (direction == -1) ? 0 : width - 1);

    direction *= -1;
    on_screen = Move(dpy, direction);
    ASSERT_TRUE(on_screen);
    QueryPointerPosition(dpy, &x, &y);
    ASSERT_EQ(x, (direction == -1) ? 0 : width - 1);
}

TEST_P(XineramaTest, ScreenCrossingButtons)
{
    XORG_TESTCASE("Start a Xinerama server with two screens."
                  "Create a window on the left and one on the right edge.\n"
                  "Move pointer to the left window, click.\n"
                  "Expect enter notify, button events on left window.\n"
                  "Move pointer to the right window, click.\n"
                  "Expect leave notify on left window, enter notify, button events on left window.\n");
    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    XSynchronize(dpy, True);
    ASSERT_TRUE(dpy);
    ASSERT_EQ(ScreenCount(dpy), 1);

    int width = DisplayWidth(dpy, DefaultScreen(dpy));
    int height = DisplayHeight(dpy, DefaultScreen(dpy));

    Window lwin = CreateWindow(dpy, None, 0, 0, 200, height);
    Window rwin = CreateWindow(dpy, None, width - 200, 0, 200, height);

    XSelectInput(dpy, lwin, EnterWindowMask|LeaveWindowMask|ButtonPressMask|ButtonReleaseMask);
    XSelectInput(dpy, rwin, EnterWindowMask|LeaveWindowMask|ButtonPressMask|ButtonReleaseMask);

    WarpPointer(dpy, 201, height/2);
    dev->PlayOne(EV_REL, REL_X, -10, true);

    ASSERT_EVENT(XEnterWindowEvent, lwin_enter, dpy, EnterNotify);
    ASSERT_EQ(lwin_enter->window, lwin);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    ASSERT_EVENT(XButtonPressedEvent, lwin_press, dpy, ButtonPress);
    ASSERT_EQ(lwin_enter->window, lwin);
    ASSERT_EVENT(XButtonPressedEvent, lwin_release, dpy, ButtonRelease);
    ASSERT_EQ(lwin_enter->window, lwin);

    dev->PlayOne(EV_REL, REL_X, width, true);

    ASSERT_EVENT(XLeaveWindowEvent, lwin_leave, dpy, LeaveNotify);
    ASSERT_EQ(lwin_enter->window, lwin);

    ASSERT_EVENT(XEnterWindowEvent, rwin_enter, dpy, EnterNotify);
    ASSERT_EQ(rwin_enter->window, rwin);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    ASSERT_EVENT(XButtonPressedEvent, rwin_press, dpy, ButtonPress);
    ASSERT_EQ(rwin_enter->window, rwin);
    ASSERT_EVENT(XButtonPressedEvent, rwin_release, dpy, ButtonRelease);
    ASSERT_EQ(rwin_enter->window, rwin);
}

INSTANTIATE_TEST_CASE_P(, XineramaTest, ::testing::Values(true, false));

class ZaphodTouchDeviceChangeTest : public MultiheadTest,
                                    public DeviceInterface {
protected:
    virtual void WriteConfig(bool left_of, bool xinerama) {
        config_path = server.GetConfigPath();
        std::ofstream config(config_path.c_str());
        config << ""
            "Section \"ServerLayout\"\n"
            "	Identifier     \"X.org Configured\"\n"
            "	Screen         0 \"Screen0\"\n"
            "	Screen         1 \"Screen1\" " << ((left_of) ? "LeftOf" : "RightOf") << " \"Screen0\"\n"
            "	Option         \"Xinerama\" \"" << (xinerama ? "on" : "off") << "\"\n"
            "	InputDevice    \"mouse\" \"CorePointer\"\n"
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
            "	Identifier  \"mouse\"\n"
            "	Driver      \"evdev\"\n"
            "	Option      \"AccelerationProfile\" \"-1\"\n"
            "	Option      \"CorePointer\" \"on\"\n"
            "	Option \"Device\" \"" + mouse->GetDeviceNode() + "\"\n"
            "EndSection\n";
        config.close();
        config.close();
    }

    virtual void SetUp() {
        SetDevice("tablets/N-Trig-MultiTouch.desc");

        mouse = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "/mice/PIXART-USB-OPTICAL-MOUSE.desc")
                );

        MultiheadTest::SetUp();
    }

    std::auto_ptr<xorg::testing::evemu::Device> mouse;
};

TEST_F(ZaphodTouchDeviceChangeTest, NoCursorJumpsOnTouchToPointerSwitch)
{
    XORG_TESTCASE("Create a touch device and a mouse device.\n"
                  "Move pointer to bottom right corner through the mouse\n"
                  "Touch and release from the touch device\n"
                  "Move pointer\n"
                  "Expect pointer motion close to touch end coordinates\n");

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());

    double x, y;
    do {
        mouse->PlayOne(EV_REL, REL_X, 10, false);
        mouse->PlayOne(EV_REL, REL_Y, 10, true);
        QueryPointerPosition(dpy, &x, &y);
    } while(x < DisplayWidth(dpy, DefaultScreen(dpy)) - 1 &&
            y < DisplayHeight(dpy, DefaultScreen(dpy))- 1);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    double touch_x, touch_y;

    QueryPointerPosition(dpy, &touch_x, &touch_y);

    /* check if we did move the pointer with the touch */
    ASSERT_NE(x, touch_x);
    ASSERT_NE(y, touch_y);

    mouse->PlayOne(EV_REL, REL_X, 1, true);
    QueryPointerPosition(dpy, &x, &y);

    ASSERT_EQ(touch_x + 1, x);
    ASSERT_EQ(touch_y, y);
}
