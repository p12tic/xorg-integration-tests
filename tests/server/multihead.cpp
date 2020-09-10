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
#include "xit-server-input-test.h"
#include "xit-event.h"
#include "helpers.h"
#include "device-emulated-interface.h"

using namespace xorg::testing;

class MultiheadTest : public XITServerInputTest,
                      public DeviceEmulatedInterface {
public:
    void SetUpConfigAndLog() override {
        config.SetAutoAddDevices(true);
        config.AddInputSection("emulated", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n" +
                               Dev(0).GetOptions());
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("emulated", "--keyboard-device--",
                               "Option \"CoreKeyboard\" \"on\"\n" +
                               Dev(1).GetOptions());
        config.AddServerLayoutOption("    Screen         0 \"Screen0\"");
        config.AddServerLayoutOption(std::string() +
            "    Screen         1 \"Screen1\" " + (left_of ? "LeftOf" : "RightOf") + " \"Screen0\"");
        config.AddServerLayoutOption(std::string() +
            "	Option         \"Xinerama\" \"" +  (xinerama ? "on" : "off") +"\"");
        config.AppendRawConfig(std::string() +
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
            "EndSection");
        config.WriteConfig();
    }

    virtual void SetUp() {
        AddDevice(xorg::testing::emulated::DeviceType::POINTER);
        AddDevice(xorg::testing::emulated::DeviceType::KEYBOARD);
        XITServerInputTest::SetUp();
    }

    void StartServer() override {
        XITServerInputTest::StartServer();
        WaitOpen();
    }

protected:
    bool xinerama;
    bool left_of;
};

class ZaphodTest : public MultiheadTest,
                   public ::testing::WithParamInterface<bool> {
public:
    virtual void SetUp() {
        xinerama = false;
        left_of = GetParam();
        MultiheadTest::SetUp();
    }

    void VerifyLeaveEvent(::Display *dpy) {
        XSync(dpy, False);

        ASSERT_GT(XPending(dpy), 0);

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
        Dev(0).PlayRelMotion(direction * 30, 0);
        XSync(dpy, False);
    }

    void FlushMotionEvents(::Display *dpy, int direction)
    {
        int limit = 0;
        Dev(0).PlayRelMotion(direction * 30, 0);
        XSync(dpy, False);
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            ASSERT_EQ(ev.type, MotionNotify);

            if (limit++ > 50) {
                ADD_FAILURE() << "Expected events to stop";
                return;
            }
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
        int limit = 0;
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
            if (limit++ > 300) {
                ADD_FAILURE() << "Expected to hit the screen edge\n";
                return false;
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

    Dev(0).PlayRelMotion(-width * 2, 0);

    ASSERT_EVENT(XEnterWindowEvent, lwin_enter, ldpy, EnterNotify);
    ASSERT_EQ(lwin_enter->window, lwin);

    Dev(0).PlayButtonDown(1);
    Dev(0).PlayButtonUp(1);

    ASSERT_EVENT(XButtonPressedEvent, lwin_press, ldpy, ButtonPress);
    ASSERT_EQ(lwin_enter->window, lwin);
    ASSERT_EVENT(XButtonPressedEvent, lwin_release, ldpy, ButtonRelease);
    ASSERT_EQ(lwin_enter->window, lwin);

    Dev(0).PlayRelMotion(width * 2, 0);

    ASSERT_EVENT(XLeaveWindowEvent, lwin_leave, ldpy, LeaveNotify);
    ASSERT_EQ(lwin_enter->window, lwin);

    ASSERT_EVENT(XEnterWindowEvent, rwin_enter, rdpy, EnterNotify);
    ASSERT_EQ(rwin_enter->window, rwin);

    Dev(0).PlayButtonDown(1);
    Dev(0).PlayButtonUp(1);

    ASSERT_EVENT(XButtonPressedEvent, rwin_press, rdpy, ButtonPress);
    ASSERT_EQ(rwin_enter->window, rwin);
    ASSERT_EVENT(XButtonPressedEvent, rwin_release, rdpy, ButtonRelease);
    ASSERT_EQ(rwin_enter->window, rwin);
}

INSTANTIATE_TEST_CASE_P(, ZaphodTest, ::testing::Values(true, false));

class XineramaTest : public ZaphodTest {
public:
    virtual void SetUp() {
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
    Dev(0).PlayRelMotion(-10, 0);

    ASSERT_EVENT(XEnterWindowEvent, lwin_enter, dpy, EnterNotify);
    ASSERT_EQ(lwin_enter->window, lwin);

    Dev(0).PlayButtonDown(1);
    Dev(0).PlayButtonUp(1);

    ASSERT_EVENT(XButtonPressedEvent, lwin_press, dpy, ButtonPress);
    ASSERT_EQ(lwin_enter->window, lwin);
    ASSERT_EVENT(XButtonPressedEvent, lwin_release, dpy, ButtonRelease);
    ASSERT_EQ(lwin_enter->window, lwin);

    Dev(0).PlayRelMotion(width, 0);

    ASSERT_EVENT(XLeaveWindowEvent, lwin_leave, dpy, LeaveNotify);
    ASSERT_EQ(lwin_enter->window, lwin);

    ASSERT_EVENT(XEnterWindowEvent, rwin_enter, dpy, EnterNotify);
    ASSERT_EQ(rwin_enter->window, rwin);

    Dev(0).PlayButtonDown(1);
    Dev(0).PlayButtonUp(1);

    ASSERT_EVENT(XButtonPressedEvent, rwin_press, dpy, ButtonPress);
    ASSERT_EQ(rwin_enter->window, rwin);
    ASSERT_EVENT(XButtonPressedEvent, rwin_release, dpy, ButtonRelease);
    ASSERT_EQ(rwin_enter->window, rwin);
}

INSTANTIATE_TEST_CASE_P(, XineramaTest, ::testing::Values(true, false));

class ZaphodTouchDeviceChangeTest : public MultiheadTest {
protected:
    virtual void SetUpConfigAndLog() {
        config.SetAutoAddDevices(true);
        config.AddInputSection("emulated", "--device-touch--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n" +
                               Dev(0).GetOptions());
        config.AddInputSection("emulated", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n" +
                               Dev(1).GetOptions());
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("emulated", "--keyboard-device--",
                               "Option \"CoreKeyboard\" \"on\"\n" +
                               Dev(2).GetOptions());
        config.AddServerLayoutOption("    Screen         0 \"Screen0\"");
        config.AddServerLayoutOption(std::string() +
            "    Screen         1 \"Screen1\" " + (left_of ? "LeftOf" : "RightOf") + " \"Screen0\"");
        config.AddServerLayoutOption(std::string() +
            "	Option         \"Xinerama\" \"" +  (xinerama ? "on" : "off") +"\"");
        config.AppendRawConfig(std::string() +
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
            "EndSection\n");
        config.WriteConfig();
    }

    virtual void SetUp() {
        AddDevice(xorg::testing::emulated::DeviceType::TOUCH);
        AddDevice(xorg::testing::emulated::DeviceType::POINTER);
        AddDevice(xorg::testing::emulated::DeviceType::KEYBOARD);
        XITServerInputTest::SetUp();
    }

    xorg::testing::emulated::Device& TouchDev() { return Dev(0); }
    xorg::testing::emulated::Device& MouseDev() { return Dev(1); }
    xorg::testing::emulated::Device& KeyboardDev() { return Dev(2); }
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
        MouseDev().PlayRelMotion(10, 10);
        QueryPointerPosition(dpy, &x, &y);
    } while(x < DisplayWidth(dpy, DefaultScreen(dpy)) - 1 &&
            y < DisplayHeight(dpy, DefaultScreen(dpy))- 1);

    TouchDev().PlayTouchBegin(200, 200, 0);
    TouchDev().PlayTouchEnd(200, 200, 0);

    double touch_x, touch_y;

    QueryPointerPosition(dpy, &touch_x, &touch_y);

    /* check if we did move the pointer with the touch */
    ASSERT_NE(x, touch_x);
    ASSERT_NE(y, touch_y);

    MouseDev().PlayRelMotion(1, 0);
    QueryPointerPosition(dpy, &x, &y);

    ASSERT_LE(std::abs(touch_x - x), 2);
    ASSERT_EQ(touch_y, y);
}
