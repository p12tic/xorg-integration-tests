#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <limits.h>

#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>

#include "xorg-conf.h"
#include "xit-server.h"
#include "helpers.h"
#include "device-interface.h"

using namespace xorg::testing;

class ZaphodTest : public Test,
                   public DeviceInterface,
                   public ::testing::WithParamInterface<bool>,
                   private ::testing::EmptyTestEventListener {
public:
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE.desc");
        InitDefaultLogFiles(server);
        config_path = server.GetConfigPath();

        bool left_of = GetParam();

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
            XNextEvent(dpy, &ev);

            switch(ev.type) {
                case LeaveNotify:
                    on_screen = false;
                    XPutBackEvent(dpy, &ev);
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
                    break;
            }
        } while(on_screen &&
                old_x < (DisplayWidth(dpy, 0) - 1) &&
                old_x != 0 && ev.xmotion.x_root != 0);

        return on_screen;
    }

protected:
    XITServer server;
    bool xinerama;

private:
    std::string config_path;
    std::string log_path;
    bool failed;
};

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
    ASSERT_TRUE(dpy);
    ASSERT_EQ(ScreenCount(dpy), 2);

    XSelectInput(dpy, RootWindow(dpy, 0), PointerMotionMask | LeaveWindowMask | EnterWindowMask);
    XSelectInput(dpy2, RootWindow(dpy2, 1), PointerMotionMask | EnterWindowMask | LeaveWindowMask);
    XSync(dpy, False);
    XSync(dpy2, False);

    bool left_of = GetParam();
    int direction = left_of ? -1 : 1;
    bool on_screen;

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

    /* We're on the second screen now, move back */
    direction *= -1;
    on_screen = Move(dpy2, direction);

    ASSERT_FALSE(on_screen) << "Expected pointer to leave screen";
    VerifyLeaveEvent(dpy2);
    on_screen = false;

    VerifyEnterEvent(dpy);
    FlushMotionEvents(dpy, direction);
}

INSTANTIATE_TEST_CASE_P(, ZaphodTest, ::testing::Values(true, false));

class XineramaTest : public ZaphodTest {
public:
    virtual void SetUp() {
        xinerama = true;
        ZaphodTest::SetUp();
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

    bool left_of = GetParam();
    int direction = left_of ? -1 : 1;
    bool on_screen = Move(dpy, direction);
    ASSERT_TRUE(on_screen);
    on_screen = Move(dpy, -direction);
    ASSERT_TRUE(on_screen);
    on_screen = Move(dpy, direction);
    ASSERT_TRUE(on_screen);
}

INSTANTIATE_TEST_CASE_P(, XineramaTest, ::testing::Values(true, false));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
