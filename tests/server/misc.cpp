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

#include <fstream>

#include <xorg/gtest/xorg-gtest.h>
#include <X11/extensions/scrnsaver.h>
#include <X11/extensions/sync.h>

#include "xit-server-input-test.h"
#include "xit-event.h"
#include "device-interface.h"
#include "helpers.h"

using namespace xorg::testing;

class EventQueueTest : public XITServerInputTest,
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

TEST_F(EventQueueTest, mieqOverflow)
{
    XORG_TESTCASE("Overflow the event queue and force a resize.\n"
                  "Search the server log for the error message.\n");

    for (int i = 0; i < 2048; i++)
        dev->PlayOne(EV_REL, REL_X, -1, true);
    XSync(Display(), False);
    ASSERT_EQ(server.GetState(), xorg::testing::Process::RUNNING);

    std::ifstream logfile(server.GetLogFilePath().c_str());
    std::string line;
    std::string bug_warn("EQ overflowing");
    bool found = false;
    ASSERT_TRUE(logfile.is_open());
    while(!found && getline(logfile, line))
        found = line.find(bug_warn);

    ASSERT_TRUE(found) << "Expected message '" << bug_warn << "' in the log "
                       << "but couldn't find it";
}

TEST(MiscServerTest, DoubleSegfault)
{
    XORG_TESTCASE("TESTCASE: SIGSEGV the server. The server must catch the "
                  "signal, clean up and then call abort().\n");

    XITServer server;
    server.Start();

    ASSERT_EQ(server.GetState(), xorg::testing::Process::RUNNING);

    /* We connect and run XSync, this usually takes long enough for some
       devices to come up. That way, input devices are part of the close
       down procedure */
    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    XSync(dpy, False);

    kill(server.Pid(), SIGSEGV);

    int status = 1;
    while(WIFEXITED(status) == false)
        if (waitpid(server.Pid(), &status, 0) == -1)
            break;

    ASSERT_TRUE(WIFSIGNALED(status));
    int termsig = WTERMSIG(status);
    ASSERT_EQ(termsig, SIGABRT)
        << "Expected SIGABRT, got " << termsig << " (" << strsignal(termsig) << ")";

    std::ifstream logfile(server.GetLogFilePath().c_str());
    std::string line;
    std::string signal_caught_msg("Caught signal");
    int signal_count = 0;
    ASSERT_TRUE(logfile.is_open());
    while(getline(logfile, line)) {
        if (line.find(signal_caught_msg) != std::string::npos)
            signal_count++;
    }

    ASSERT_EQ(signal_count, 1) << "Expected one signal to show up in the log\n";

    unlink(server.GetLogFilePath().c_str());

    /* The XServer destructor tries to terminate the server and prints
       warnings if it fails */
    std::cout << "Note: below warnings about server termination failures server are false positives\n";
    server.Terminate(); /* explicitly terminate instead of destructor to
                           only get one warning */
}

class ScreenSaverTest : public XITServerInputTest,
                        public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        XITServerInputTest::SetUp();

        ASSERT_TRUE(XScreenSaverQueryExtension(Display(), &screensaver_opcode, &screensaver_error_base));
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

    int screensaver_opcode;
    int screensaver_error_base;
};


/* Screen saver activation takes a while, so this test does more things to
   avoid slowing down the test suite too much */
TEST_F(ScreenSaverTest, ScreenSaverActivateDeactivate)
{
    XORG_TESTCASE("Set screen saver timeout to 1 second.\n"
                  "Register for ScreensaverNotify events.\n"
                  "Wait 1 second\n"
                  "Ensure event is received with state on.\n"
                  "Query server for screensaver info, make sure saver is on\n"
                  "Force screen saver off\n"
                  "Ensure event is received with state off\n"
                  "Query server for screensaver info, make sure saver is off\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=56649"
                  );

    ::Display *dpy = Display();
    XScreenSaverSelectInput(dpy, DefaultRootWindow(dpy), ScreenSaverNotifyMask);
    XSetScreenSaver(dpy, 1, 0, DontPreferBlanking, DefaultExposures);
    XSync(dpy, False);

    EXPECT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           screensaver_opcode + ScreenSaverNotify,
                                                           -1,
                                                           -1,
                                                           2000));
    XEvent ev;
    XNextEvent(dpy, &ev);
    XScreenSaverNotifyEvent *scev = reinterpret_cast<XScreenSaverNotifyEvent*>(&ev);
    EXPECT_EQ(scev->state, ScreenSaverOn);

    XScreenSaverInfo info;
    XScreenSaverQueryInfo(dpy, DefaultRootWindow(dpy), &info);
    ASSERT_EQ(info.state, ScreenSaverOn);
    EXPECT_GT(info.til_or_since, 0U);

    XResetScreenSaver(dpy);
    XSync(dpy, False);

    EXPECT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           screensaver_opcode + ScreenSaverNotify,
                                                           -1,
                                                           -1,
                                                           2000));
    XNextEvent(dpy, &ev);
    EXPECT_EQ(scev->state, ScreenSaverOff);

    XScreenSaverQueryInfo(dpy, DefaultRootWindow(dpy), &info);
    ASSERT_EQ(info.state, ScreenSaverOff);
    EXPECT_LE(info.til_or_since, 1000U);
    EXPECT_LT(info.idle, 1000U);
}


class XSyncTest : public XITServerInputTest {
    public:
    virtual void SetUp() {
        XITServerInputTest::SetUp();
        QuerySyncExtension(Display());
    }

    virtual void QuerySyncExtension(::Display *dpy) {
        ASSERT_TRUE(XSyncQueryExtension (dpy, &sync_event, &sync_error));
        ASSERT_TRUE(XSyncInitialize (dpy, &sync_major, &sync_minor));
    }

    virtual XSyncAlarm SetAbsoluteAlarm(::Display *dpy, XSyncCounter counter,
                                        int threshold, XSyncTestType direction,
                                        bool need_events = true) {
        XSyncAlarm alarm;
        int flags;
        XSyncAlarmAttributes attr;
        XSyncValue delta, interval;

        XSyncIntToValue(&delta, 0);
        XSyncIntToValue(&interval, threshold);

        attr.trigger.counter = counter;
        attr.trigger.test_type = direction;;
        attr.trigger.value_type = XSyncAbsolute;
        attr.trigger.wait_value = interval;
        attr.delta = delta;
        attr.events = need_events;

        flags = XSyncCACounter | XSyncCAValueType | XSyncCATestType |
                XSyncCAValue | XSyncCADelta| XSyncCAEvents;

        alarm = XSyncCreateAlarm(dpy, flags, &attr);
        XSync(dpy, False);
        return alarm;
    }

    int sync_event;
    int sync_error;
    int sync_major;
    int sync_minor;
};

class IdletimerTest : public XSyncTest,
                      public DeviceInterface {
public:
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        XSyncTest::SetUp();
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

    virtual XSyncCounter GetIdletimeCounter(::Display *dpy) {
        int ncounters;
        XSyncSystemCounter *counters;
        XSyncCounter counter = None;

        counters = XSyncListSystemCounters (dpy, &ncounters);
        for (int i = 0; i < ncounters; i++) {
            if (!strcmp(counters[i].name, "IDLETIME")) {
                counter = counters[i].counter;
            }
        }

        XSyncFreeSystemCounterList(counters);

        if (!counter)
            ADD_FAILURE() << "IDLETIME counter not found.";
        return counter;
    }

    /**
     * This function is necessary because in this test setup the server may
     * hang when we're only sending one event. Event gets processed, but
     * ProcessInputEvent() is never called so we just hang waiting for
     * something to happen and the test fails.
     * This is a server bug, but won't be triggered on a normal desktop
     * since there's always something to process (client request, timer,
     * etc.).
     */
    virtual void WaitForEvent(::Display *dpy) {
        XEvent ev;
        XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask);
        while (!XCheckMaskEvent(dpy, PointerMotionMask, &ev)) {
            dev->PlayOne(EV_REL, REL_X, 10, true);
            XSync(dpy, False);
            usleep(10000);
        }
    }
};

TEST_F(IdletimerTest, NegativeTransitionHighThreshold)
{
    XORG_TESTCASE("Set up an alarm on negative transition for a high threshold\n"
                  "Wait past threshold, then move pointer\n"
                  "Expect event\n"
                  "Wait past threshold, then move pointer\n"
                  "Expect event\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=59644");

    ::Display *dpy = Display();

    /* make sure server is ready to send events */
    WaitForEvent(dpy);
    XSync(dpy, True);

    XSyncAlarm neg_alarm;
    XSyncCounter idlecounter;

    /* bug: if the negative transition threshold fires but the idletime is
       below the threshold, it is never set up again */

    const int threshold = 1000; /* ms  */

    idlecounter = GetIdletimeCounter(dpy);
    ASSERT_GT(idlecounter, (XSyncCounter)None);
    neg_alarm = SetAbsoluteAlarm(dpy, idlecounter, threshold, XSyncNegativeTransition);
    ASSERT_GT(neg_alarm, (XSyncAlarm)None);

    usleep(threshold * 1.5 * 1000);
    WaitForEvent(dpy);

    usleep(threshold * 1.5 * 1000);
    WaitForEvent(dpy);

    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev1, dpy, sync_event + XSyncAlarmNotify);
    ASSERT_EQ(ev1->alarm, neg_alarm);
    ASSERT_EQ(ev1->state, XSyncAlarmActive);

    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev2, dpy, sync_event + XSyncAlarmNotify);
    ASSERT_EQ(ev2->alarm, neg_alarm);
    ASSERT_EQ(ev1->state, XSyncAlarmActive);
}

TEST_F(IdletimerTest, NegativeTransitionLowThreshold)
{
    XORG_TESTCASE("Set up an alarm on negative transition for a low threshold\n"
                  "Wait past threshold, then move pointer\n"
                  "Expect alarm event\n"
                  "Wait past threshold, then move pointer\n"
                  "Expect alarm event\n"
                  "------------------------------------------------\n"
                  "This alarm will generate false positives, the test \n"
                  "will only fail if the server is busy and the time to\n"
                  "idle timer query takes more than <threshold> ms\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=70476");

    ::Display *dpy = Display();

    WaitForEvent(dpy);
    XSync(dpy, True);

    XSyncAlarm neg_alarm;
    XSyncCounter idlecounter;

    /* bug: if the negative transition threshold fires but the idletime may
       move past the threshold before the handler queries it */

    const int threshold = 1; /* ms  */

    usleep(threshold * 1000);

    idlecounter = GetIdletimeCounter(dpy);
    ASSERT_GT(idlecounter, (XSyncCounter)None);
    neg_alarm = SetAbsoluteAlarm(dpy, idlecounter, threshold, XSyncNegativeTransition);
    ASSERT_GT(neg_alarm, (XSyncAlarm)None);

    usleep(threshold * 1.5 * 1000);
    WaitForEvent(dpy);

    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev1, dpy, sync_event + XSyncAlarmNotify);
    ASSERT_EQ(ev1->alarm, neg_alarm);
    ASSERT_EQ(ev1->state, XSyncAlarmActive);
}

TEST_F(IdletimerTest, PositiveTransitionLowThreshold)
{
    XORG_TESTCASE("Set up an alarm on positive transition for a low threshold\n"
                  "Wait past threshold, then move pointer\n"
                  "Expect event when idletime passes threshold\n"
                  "Wait past threshold, then move pointer\n"
                  "Expect event when idletime passes threshold\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=70476");

    ::Display *dpy = Display();

    /* make sure server is ready to send events */
    WaitForEvent(dpy);
    XSync(dpy, True);

    XSyncAlarm pos_alarm;
    XSyncCounter idlecounter;

    /* bug: if the postive transition threshold fires but the idletime is
       already above the threshold, it is never set up again */

    const int threshold = 1; /* ms  */

    usleep(threshold * 20000);

    idlecounter = GetIdletimeCounter(dpy);
    ASSERT_GT(idlecounter, (XSyncCounter)None);
    pos_alarm = SetAbsoluteAlarm(dpy, idlecounter, threshold, XSyncPositiveTransition);
    SetAbsoluteAlarm(dpy, idlecounter, threshold, XSyncNegativeTransition, false);
    ASSERT_GT(pos_alarm, (XSyncAlarm)None);

    WaitForEvent(dpy);
    usleep(threshold * 2000);

    WaitForEvent(dpy);
    usleep(threshold * 2000);

    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev1, dpy, sync_event + XSyncAlarmNotify);
    ASSERT_EQ(ev1->alarm, pos_alarm);
    ASSERT_EQ(ev1->state, XSyncAlarmActive);

    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev2, dpy, sync_event + XSyncAlarmNotify);
    ASSERT_EQ(ev2->alarm, pos_alarm);
    ASSERT_EQ(ev1->state, XSyncAlarmActive);
}

TEST_F(IdletimerTest, NegativeTransition)
{
    XORG_TESTCASE("Set up an alarm on negative transition\n"
                  "Move pointer\n"
                  "Expect event\n"
                  "Wait for timeout\n"
                  "Move pointer again\n"
                  "Expect event\n");

    ::Display *dpy = Display();

    /* make sure server is ready to send events */
    WaitForEvent(dpy);
    XSync(dpy, True);

    XSyncAlarm neg_alarm;
    XSyncCounter idlecounter;

    /* bug: if the negative transition threshold fires but the idletime is
       below the threshold, it is never set up again */
    const int threshold = 5;
    usleep(threshold * 1200);

    idlecounter = GetIdletimeCounter(dpy);
    ASSERT_GT(idlecounter, (XSyncCounter)None);
    neg_alarm = SetAbsoluteAlarm(dpy, idlecounter, threshold, XSyncNegativeTransition);
    ASSERT_GT(neg_alarm, (XSyncAlarm)None);

    dev->PlayOne(EV_REL, REL_X, 10, true);
    WaitForEvent(dpy);
    usleep(threshold * 1200);

    dev->PlayOne(EV_REL, REL_X, 10, true);
    WaitForEvent(dpy);

    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev1, dpy, sync_event + XSyncAlarmNotify);
    ASSERT_EQ(ev1->alarm, neg_alarm);
    ASSERT_EQ(ev1->state, XSyncAlarmActive);

    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev2, dpy, sync_event + XSyncAlarmNotify);
    ASSERT_EQ(ev2->alarm, neg_alarm);
    ASSERT_EQ(ev1->state, XSyncAlarmActive);
}

TEST_F(IdletimerTest, NegativeTransitionMultipleAlarms)
{
    XORG_TESTCASE("Set up an multiple alarm on negative transitions\n"
                  "Move pointer\n"
                  "Expect event\n"
                  "Wait for timeout\n"
                  "Move pointer again\n"
                  "Expect event\n");

    ::Display *dpy = Display();

    /* make sure server is ready to send events */
    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask);
    WaitForEvent(dpy);
    XSync(dpy, True);

    XSyncAlarm neg_alarm1, neg_alarm2;
    XSyncCounter idlecounter;

    /* bug: if the negative transition threshold fires but the idletime is
       below the threshold, it is never set up again */
    const int threshold = 5;
    usleep(threshold * 2000);

    idlecounter = GetIdletimeCounter(dpy);
    ASSERT_GT(idlecounter, (XSyncCounter)None);
    neg_alarm2 = SetAbsoluteAlarm(dpy, idlecounter, threshold/2, XSyncNegativeTransition);
    neg_alarm1 = SetAbsoluteAlarm(dpy, idlecounter, threshold, XSyncNegativeTransition);
    ASSERT_GT(neg_alarm1, (XSyncAlarm)None);
    ASSERT_GT(neg_alarm2, (XSyncAlarm)None);

    WaitForEvent(dpy);
    usleep(threshold * 2000);

    WaitForEvent(dpy);

    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev1, dpy, sync_event + XSyncAlarmNotify);
    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev2, dpy, sync_event + XSyncAlarmNotify);
    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev3, dpy, sync_event + XSyncAlarmNotify);
    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev4, dpy, sync_event + XSyncAlarmNotify);
}

TEST_F(IdletimerTest, NegativeTransitionToZero)
{
    XORG_TESTCASE("Set up an alarm on negative transition to 0\n"
                  "Move pointer\n"
                  "Expect event\n"
                  "Wait for timeout\n"
                  "Move pointer again\n"
                  "Expect event\n");

    ::Display *dpy = Display();

    /* make sure server is ready to send events */
    WaitForEvent(dpy);
    XSync(dpy, True);

    XSyncAlarm neg_alarm;
    XSyncCounter idlecounter;

    /* bug: if the negative transition threshold fires but the idletime is
       below the threshold, it is never set up again */
    const int threshold = 0;
    usleep(10000);

    idlecounter = GetIdletimeCounter(dpy);
    ASSERT_GT(idlecounter, (XSyncCounter)None);
    neg_alarm = SetAbsoluteAlarm(dpy, idlecounter, threshold, XSyncNegativeTransition);
    ASSERT_GT(neg_alarm, (XSyncAlarm)None);

    dev->PlayOne(EV_REL, REL_X, 10, true);
    WaitForEvent(dpy);
    usleep(10000);

    dev->PlayOne(EV_REL, REL_X, 10, true);
    WaitForEvent(dpy);

    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev1, dpy, sync_event + XSyncAlarmNotify);
    ASSERT_EQ(ev1->alarm, neg_alarm);
    ASSERT_EQ(ev1->state, XSyncAlarmActive);

    ASSERT_EVENT(XSyncAlarmNotifyEvent, ev2, dpy, sync_event + XSyncAlarmNotify);
    ASSERT_EQ(ev2->alarm, neg_alarm);
    ASSERT_EQ(ev1->state, XSyncAlarmActive);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
