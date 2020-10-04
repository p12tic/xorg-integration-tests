/*
 * Copyright © 2020 Povilas Kanapickas <povilas@radix.lt>
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

#include "helpers.h"
#include "gestures-common.h"

#if HAVE_XI24

enum GrabType {
    GRABTYPE_CORE,
    GRABTYPE_XI1,
    GRABTYPE_XI2,
};

class GestureRootChildWindowGrabTypeTest : public GestureTest,
                                           public ::testing::WithParamInterface<std::tuple<int, int, int, int>> {
public:
    GestureRootChildWindowGrabTypeTest() {
        SetIsPinch(std::get<0>(GetParam()) == XI_GesturePinchBegin);
    }
    int Window1Depth() const { return std::get<1>(GetParam()); }
    int Window2Depth() const { return std::get<2>(GetParam()); }
    GrabType GetGrabType() const { return static_cast<GrabType>(std::get<3>(GetParam())); }
};

TEST_P(GestureRootChildWindowGrabTypeTest, ActiveGrabOverGestureSelection)
{
    XORG_TESTCASE("Client C1 creates window and selects for touchpad gestures.\n"
                  "Client C2 has async active grab\n"
                  "Play a gesture on the window\n"
                  "Gestures events should not come to any client\n"
                  "This should hold regardless of whether grab or selection happened on the\n"
                  "root or the child window\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = CreateWindow(dpy1, None);

    Window select_win = Window1Depth() == 0 ? DefaultRootWindow(dpy1) : win;
    Window grab_win = Window2Depth() == 0 ? DefaultRootWindow(dpy2) : win;

    SelectXI2Events(dpy1, XIAllMasterDevices, select_win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    EventMaskBuilder grab_mask{VIRTUAL_CORE_POINTER_ID, { XI_Motion }};

    int deviceid;
    FindInputDeviceByName(dpy2, "--touchpad-device--", &deviceid);
    XDevice *xdev = XOpenDevice(dpy2, deviceid);
    XEventClass cls;
    int xi_motion;
    DeviceMotionNotify(xdev, xi_motion, cls);

    // XI1 does not allow us to grab main pointer.
    switch(GetGrabType()) {
        case GRABTYPE_CORE:
            ASSERT_EQ(Success, XGrabPointer(dpy2, grab_win, False, PointerMotionMask,
                                            GrabModeAsync, GrabModeAsync, None, None,
                                            CurrentTime));
            break;
        case GRABTYPE_XI2:
            ASSERT_EQ(Success, XIGrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, grab_win, CurrentTime,
                                            None, GrabModeAsync, GrabModeAsync, False,
                                            grab_mask.GetMaskPtr()));
            break;
    }

    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XCloseDisplay(dpy2);
}

INSTANTIATE_TEST_CASE_P(, GestureRootChildWindowGrabTypeTest,
                        ::testing::Combine(
                            ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin),
                            ::testing::Values(0, 1),
                            ::testing::Values(0, 1),
                            ::testing::Values(GRABTYPE_CORE, GRABTYPE_XI2)));

class GestureGrabRootChildWindowTest : public GestureRootChildWindowTest {};

TEST_P(GestureGrabRootChildWindowTest, ActiveGestureGrabOverGestureSelection)
{
    XORG_TESTCASE("Client C1 creates window and selects for touchpad gestures.\n"
                  "Client C2 has async active gesture\n"
                  "Play a gesture on the window\n"
                  "Gestures should come only to the C2 client.\n"
                  "This should hold regardless of whether grab or selection happened on the\n"
                  "root or the child window\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = CreateWindow(dpy1, None);

    Window select_win = Window1Depth() == 0 ? DefaultRootWindow(dpy1) : win;
    Window grab_win = Window2Depth() == 0 ? DefaultRootWindow(dpy2) : win;

    SelectXI2Events(dpy1, XIAllMasterDevices, select_win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, grab_win, GrabModeAsync,
               { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_EVENT(void, gesture11, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, gesture12, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, gesture13, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());

    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_EVENT(void, gesture21, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, gesture22, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, gesture23, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());

    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy1));

    XCloseDisplay(dpy2);
}

TEST_P(GestureGrabRootChildWindowTest, PassiveMatchingGestureGrabOverGestureSelection)
{
    XORG_TESTCASE("Client C1 creates window and selects for touchpad gestures.\n"
                  "Client C2 creates a passive gesture grab\n"
                  "Play gesture equence that triggers the grab\n"
                  "Gestures should come only to the C2 client.\n"
                  "This should hold regardless of whether grab or selection happened on the\n"
                  "root or the child window\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = CreateWindow(dpy1, None);

    Window select_win = Window1Depth() == 0 ? DefaultRootWindow(dpy1) : win;
    Window grab_win = Window2Depth() == 0 ? DefaultRootWindow(dpy2) : win;

    SelectXI2Events(dpy1, XIAllMasterDevices, select_win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GrabMatchingGestureBegin(dpy2, grab_win, GrabModeAsync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_EVENT(void, gesture1, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, gesture2, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, gesture3, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());

    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy1));
}

TEST_P(GestureGrabRootChildWindowTest, PassiveNonMatchingGestureGrabOverGestureSelection)
{
    XORG_TESTCASE("Client C1 creates window and selects for touchpad gestures.\n"
                  "Client C2 creates a passive gesture grab\n"
                  "Play gesture sequence that does not trigger the grab\n"
                  "Gestures should come only to the C1 client.\n"
                  "This should hold regardless of whether grab or selection happened on the\n"
                  "root or the child window\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = CreateWindow(dpy1, None);

    Window select_win = Window1Depth() == 0 ? DefaultRootWindow(dpy1) : win;
    Window grab_win = Window2Depth() == 0 ? DefaultRootWindow(dpy2) : win;

    SelectXI2Events(dpy1, XIAllMasterDevices, select_win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GrabNonMatchingGestureBegin(dpy2, grab_win, GrabModeAsync, GrabModeAsync,
                                { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_EVENT(void, gesture1, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, gesture2, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, gesture3, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

INSTANTIATE_TEST_CASE_P(, GestureGrabRootChildWindowTest,
                        ::testing::Combine(
                            ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin),
                            ::testing::Values(0, 1),
                            ::testing::Values(0, 1)));

class GesturePassiveGrab : public GestureTypesTest {};

TEST_P(GesturePassiveGrab, SyncGrabReplaysEventsBeforeEnd)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates a passive gesture grab\n"
                  "Play gesture sequence that does triggers the grab.\n"
                  "C2 processes gesture begin only after begin and update are queued.\n"
                  "Gesture event should come only to the C2 client.\n"
                  "C2 calls XIAllowEvents(ReplayDevice) after getting gesture begin.\n"
                  "Events should be replayed to C1.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GrabMatchingGestureBegin(dpy2, win, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GesturePlayBegin();
    GesturePlayUpdate();

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XIReplayDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c1_begin, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c1_update, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayEnd();

    ASSERT_EVENT(void, e_c1_end, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GesturePassiveGrab, SyncGrabReplaysEventsAtEnd)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates a passive gesture grab\n"
                  "Play gesture sequence that does triggers the grab.\n"
                  "C2 processes gesture begin only after begin, update and end are queued.\n"
                  "Gesture event should come only to the C2 client.\n"
                  "C2 calls XIAllowEvents(ReplayDevice) after getting gesture begin.\n"
                  "Events should be replayed to C1.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GrabMatchingGestureBegin(dpy2, win, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XIReplayDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c1_begin, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c1_update, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c1_end, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GesturePassiveGrab, SyncGrabIgnoresPassiveGrabsAboveWhenReplaying)
{
    XORG_TESTCASE("Client C1 creates window and selects for gestures.\n"
                  "Client C2 creates a passive gesture grab\n"
                  "Play gesture sequence that does trigger the grab.\n"
                  "Client C3 creates a passive gesture grab above the window."
                  "Gestures should come only to the C2 client.\n"
                  "C2 calls XIAllowEvents(ReplayDevice) and the events are replayed to C1.\n"
                  "No gestures should come to C3.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();
    ::Display *dpy3 = NewClient();

    Window win = DefaultRootWindow(dpy1);
    Window win2 = CreateWindow(dpy1, None);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win2,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GrabMatchingGestureBegin(dpy2, win2, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GesturePlayBegin();
    GesturePlayUpdate();

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    GrabMatchingGestureBegin(dpy3, win, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XIReplayDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c1_begin, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c1_update, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    GesturePlayEnd();

    ASSERT_EVENT(void, e_c1_end, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));
}

TEST_P(GesturePassiveGrab, SyncGrabDoesNotIgnorePassiveGrabsBelowWhenReplaying)
{
    XORG_TESTCASE("Client C1 creates window and selects for gestures.\n"
                  "Client C2 creates a passive gesture grab\n"
                  "Play gesture sequence that does trigger the grab.\n"
                  "Client C3 creates a passive gesture grab below the window."
                  "Gestures should come only to the C2 client.\n"
                  "C2 calls XIAllowEvents(ReplayPointer) and the events are replayed to C3.\n"
                  "C3 calls XIAllowEvents(ReplayPointer) and the events are replayed to C1.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();
    ::Display *dpy3 = NewClient();

    Window win = DefaultRootWindow(dpy1);
    Window win2 = CreateWindow(dpy1, None);
    Window win3 = CreateWindow(dpy1, win2);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win3,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GrabMatchingGestureBegin(dpy2, win2, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GesturePlayBegin();
    GesturePlayUpdate();

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    GrabMatchingGestureBegin(dpy3, win3, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XIReplayDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c3_begin, dpy3, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    XIAllowEvents(dpy3, VIRTUAL_CORE_POINTER_ID, XIReplayDevice, CurrentTime);

    ASSERT_EVENT(void, e_c3_end, dpy3, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c1_begin, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c1_update, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    GesturePlayEnd();

    ASSERT_EVENT(void, e_c1_end, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));
}

TEST_P(GesturePassiveGrab, SyncPointerGrabReplaysGestureEventsBeforeEnd)
{
    XORG_TESTCASE("Client C1 creates window and selects for events.\n"
                  "Client C2 creates a passive pointer grab\n"
                  "Client C3 creates a passive gesture grab below the window."
                  "Play button press that triggers the grab. Events should come to C2.\n"
                  "C2 calls XIAllowEvents(SyncDevice)"
                  "Gestures should come only to the C2 client.\n"
                  "C2 calls XIAllowEvents(ReplayDevice) and the events are replayed to C3.\n"
                  "C3 calls XIAllowEvents(ReplayDevice) and the events are replayed to C1.\n");


    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();
    ::Display *dpy3 = NewClient();

    Window win2 = CreateWindow(dpy1, None);
    Window win3 = CreateWindow(dpy1, win2);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win3,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                      XI_ButtonPress, XI_ButtonRelease, XI_Motion });

    GrabButton(dpy2, win2, 1, GrabModeSync,
               { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                 XI_ButtonPress, XI_ButtonRelease, XI_Motion });

    GrabMatchingGestureBegin(dpy3, win3, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                               XI_ButtonPress, XI_ButtonRelease, XI_Motion });

    TouchPadDev().PlayButtonDown(1); // freezes device
    TouchPadDev().PlayRelMotion(1, 0);

    ASSERT_EVENT(void, e_c2_press, dpy2, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    GesturePlayBegin(); // freezes device
    GesturePlayUpdate();

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XISyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_motion, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XIReplayDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c3_begin, dpy3, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    XIAllowEvents(dpy3, VIRTUAL_CORE_POINTER_ID, XIReplayDevice, CurrentTime);

    ASSERT_EVENT(void, e_c3_end, dpy3, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c1_begin, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c1_update, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    GesturePlayEnd();

    ASSERT_EVENT(void, e_c1_end, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));
}

TEST_P(GesturePassiveGrab, SyncPointerGrabReplaysGestureEventsAfterEnd)
{
    XORG_TESTCASE("Client C1 creates window and selects for events.\n"
                  "Client C2 creates a passive pointer grab\n"
                  "Client C3 creates a passive gesture grab below the window.\n"
                  "Play button press that triggers the grab.\n"
                  "Play a gesture sequence before anyone gets a chance to react.\n"
                  "Events should come to C2.\n"
                  "C2 calls XIAllowEvents(SyncDevice)"
                  "Gestures should come only to the C2 client.\n"
                  "C2 calls XIAllowEvents(ReplayDevice) and the events are replayed to C3.\n"
                  "C3 calls XIAllowEvents(ReplayDevice) and the events are replayed to C1.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();
    ::Display *dpy3 = NewClient();

    Window win2 = CreateWindow(dpy1, None);
    Window win3 = CreateWindow(dpy1, win2);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win3,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                      XI_ButtonPress, XI_ButtonRelease, XI_Motion });

    GrabButton(dpy2, win2, 1, GrabModeSync,
               { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                 XI_ButtonPress, XI_ButtonRelease, XI_Motion });

    GrabMatchingGestureBegin(dpy3, win3, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                               XI_ButtonPress, XI_ButtonRelease, XI_Motion });

    TouchPadDev().PlayButtonDown(1); // freezes device
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayBegin(); // freezes device second time below
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_EVENT(void, e_c2_press, dpy2, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XISyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_motion, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XIReplayDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c3_begin, dpy3, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    XIAllowEvents(dpy3, VIRTUAL_CORE_POINTER_ID, XIReplayDevice, CurrentTime);

    ASSERT_EVENT(void, e_c3_end, dpy3, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c1_begin, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c1_update, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c1_end, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));
}

TEST_P(GesturePassiveGrab, SyncGrabReplayDeviceReplaysNotGestureBeforeEnd)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates a passive gesture grab\n"
                  "Play gesture sequence that triggers the grab.\n"
                  "C2 allows events only after begin, update and motion are queued.\n"
                  "Events should come only to the C2 client.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                      XI_ButtonPress, XI_Motion, XI_ButtonRelease });

    GrabMatchingGestureBegin(dpy2, win, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                               XI_ButtonPress, XI_Motion, XI_ButtonRelease });

    GesturePlayBegin();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayUpdate();

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XISyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_motion, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c2_update, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    TouchPadDev().PlayButtonDown(1);

    ASSERT_EVENT(void, e_c2_press, dpy2, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XIReplayDevice, CurrentTime);

    ASSERT_EVENT(void, e_c1_press, dpy1, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    TouchPadDev().PlayButtonUp(1);

    ASSERT_EVENT(void, e_c1_release, dpy1, GenericEvent, xi2_opcode, XI_ButtonRelease);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayEnd();

    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GesturePassiveGrab, SyncGrabReplayDeviceReplaysNotGestureAfterEnd)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates a passive gesture grab\n"
                  "Play gesture sequence that triggers the grab.\n"
                  "C2 allows events only after begin, update and motion are queued.\n"
                  "All events should come only to the C2 client.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                      XI_ButtonPress, XI_Motion, XI_ButtonRelease });

    GrabMatchingGestureBegin(dpy2, win, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                               XI_ButtonPress, XI_Motion, XI_ButtonRelease });

    GesturePlayBegin();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayUpdate();

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XISyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_motion, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c2_update, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    TouchPadDev().PlayButtonDown(1);
    GesturePlayEnd();

    ASSERT_EVENT(void, e_c2_press, dpy2, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XIReplayDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c1_press, dpy1, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    TouchPadDev().PlayButtonUp(1);

    ASSERT_EVENT(void, e_c1_release, dpy1, GenericEvent, xi2_opcode, XI_ButtonRelease);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GesturePassiveGrab, SyncPointerGrabFreezesOnGestureBeginBeforeGestureStarts)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates a passive pointer grab\n"
                  "Press button to trigger the grab.\n"
                  "C2 allows events in synchronous mode.\n"
                  "A gesture is played to freeze the device again.\n"
                  "All events should come to the C2 client.\n"
                  "Grab only ends when the button is released\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                      XI_ButtonPress, XI_Motion, XI_ButtonRelease });

    GrabButton(dpy2, win, 1, GrabModeSync,
               { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                 XI_ButtonPress, XI_Motion, XI_ButtonRelease });

    TouchPadDev().PlayButtonDown(1);
    TouchPadDev().PlayRelMotion(1, 0);

    ASSERT_EVENT(void, e_c2_press, dpy2, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XISyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_motion1, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayBegin(); // freezes the device
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayUpdate();

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XISyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_motion2, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c2_update, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayEnd(); // freezes the device

    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    TouchPadDev().PlayRelMotion(1, 0);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XISyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_motion3, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    TouchPadDev().PlayButtonUp(1); // releases the grab
    ASSERT_EVENT(void, e_c2_release, dpy2, GenericEvent, xi2_opcode, XI_ButtonRelease);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    TouchPadDev().PlayRelMotion(1, 0);
    ASSERT_EVENT(void, e_c1_motion, dpy1, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GesturePassiveGrab, SyncPointerGrabFreezesOnGestureBeginAfterGestureStarts)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates a passive pointer grab\n"
                  "Press button to trigger the grab.\n"
                  "A gesture is played to freeze the device again after events are allowed.\n"
                  "C2 allows events in synchronous mode.\n"
                  "All events should come to the C2 client.\n"
                  "Grab only ends when the button is released\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                      XI_ButtonPress, XI_Motion, XI_ButtonRelease });

    GrabButton(dpy2, win, 1, GrabModeSync,
               { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                 XI_ButtonPress, XI_Motion, XI_ButtonRelease });

    TouchPadDev().PlayButtonDown(1); // freezes the device
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayBegin(); // freezes the device immediately after XIAllowEventsBelow
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayUpdate();

    ASSERT_EVENT(void, e_c2_press, dpy2, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XISyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_motion1, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XISyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_motion2, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c2_update, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayEnd(); // freezes the device

    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    TouchPadDev().PlayRelMotion(1, 0);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XISyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_motion3, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    TouchPadDev().PlayButtonUp(1); // releases the grab
    ASSERT_EVENT(void, e_c2_release, dpy2, GenericEvent, xi2_opcode, XI_ButtonRelease);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    TouchPadDev().PlayRelMotion(1, 0);
    ASSERT_EVENT(void, e_c1_motion, dpy1, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GesturePassiveGrab, SyncGrabGetsQueuedEventsAfterAsyncAllowBeforeEnd)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates a passive gesture grab\n"
                  "Play gesture sequence that triggers the grab.\n"
                  "C2 processes gesture begin only after some events are queued.\n"
                  "C2 allows async processing of events via AllowEvents.\n"
                  "All events go to C2 until the passive grab is deactivated.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    GrabMatchingGestureBegin(dpy2, win, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                               XI_Motion });

    GesturePlayBegin();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayUpdate();

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XIAsyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_motion, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c2_update, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayEnd();
    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    TouchPadDev().PlayRelMotion(1, 0);
    ASSERT_EVENT(void, e_c1_motion, dpy1, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GesturePassiveGrab, SyncGrabGetsQueuedEventsAfterAsyncAllowAfterEnd)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates a passive gesture grab\n"
                  "Play gesture sequence that triggers the grab.\n"
                  "C2 processes gesture begin only after some events are queued.\n"
                  "C2 allows async processing of events via AllowEvents.\n"
                  "All events go to C2 until the passive grab is deactivated.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    GrabMatchingGestureBegin(dpy2, win, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                               XI_Motion });

    GesturePlayBegin();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayUpdate();
    GesturePlayEnd();
    TouchPadDev().PlayRelMotion(1, 0);

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XIAsyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_motion, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c2_update, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c1_motion, dpy1, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GesturePassiveGrab, SyncGrabUnfreezesWhenClientExits)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates a passive gesture grab\n"
                  "Play gesture sequence that triggers the grab.\n"
                  "C2 exits. All events including the replayed ones should go to C1.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    GrabMatchingGestureBegin(dpy2, win, GrabModeSync, GrabModeAsync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(),
                               XI_Motion });

    GesturePlayBegin();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayUpdate();
    GesturePlayEnd();
    TouchPadDev().PlayRelMotion(1, 0);

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XCloseDisplay(dpy2);

    ASSERT_EVENT(void, e_c1_motion, dpy1, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c1_motion2, dpy1, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_TRUE(NoEventPending(dpy1));
}

TEST_P(GesturePassiveGrab, AsyncGrabKeyboardSync)
{
    XORG_TESTCASE("C2 creates an async passive gesture grab (keyboard mode sync) on window\n"
                  "Generate gesture begin event\n"
                  "Generate keyboard events\n"
                  "Generate gesture end event\n"
                  "Allow keyboard events.\n"
                  "Make sure all events arrive to client C2.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_KEYBOARD_ID, win,
                    { XI_KeyPress, XI_KeyRelease });
    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    SelectXI2Events(dpy2, VIRTUAL_CORE_KEYBOARD_ID, win, { XI_KeyPress, XI_KeyRelease });
    GrabMatchingGestureBegin(dpy2, win, GrabModeAsync, GrabModeSync,
                             { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GesturePlayBegin();
    KeyboardDev().PlayKeyDown(KEY_A);
    KeyboardDev().PlayKeyUp(KEY_A);

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayEnd();

    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c2_press, dpy2, GenericEvent, xi2_opcode, XI_KeyPress);
    ASSERT_EVENT(void, e_c2_release, dpy2, GenericEvent, xi2_opcode, XI_KeyRelease);
    ASSERT_EVENT(void, e_c1_press, dpy1, GenericEvent, xi2_opcode, XI_KeyPress);
    ASSERT_EVENT(void, e_c1_release, dpy1, GenericEvent, xi2_opcode, XI_KeyRelease);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

INSTANTIATE_TEST_CASE_P(, GesturePassiveGrab,
                        ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin));

class GestureActiveGrab : public GestureTypesTest {};

TEST_P(GestureActiveGrab, AsyncGrabWhenGestureAlreadyBegun)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "A gesture is begun.\n"
                  "Client C2 creates an active async gesture grab\n"
                  "Play events.\n"
                  "Events for existing gesture should go to C1, events for new gesture should go to C2.\n"
                  "C2 deactivates gesture grab\n"
                  "All events go to C1.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy2);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    GesturePlayBegin();
    ASSERT_EVENT(void, e_c1_begin, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, win, GrabModeAsync,
               { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayUpdate();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayEnd();

    ASSERT_EVENT(void, e_c1_update, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c2_motion1, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c1_end, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayBegin();
    GesturePlayUpdate();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayEnd();

    ASSERT_EVENT(void, e_c2_begin2, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c2_update2, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c2_motion2, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c2_end2, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIUngrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, CurrentTime);

    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_EVENT(void, e_c1_begin2, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c1_update2, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c1_end2, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GestureActiveGrab, SyncGrabWhenGestureAlreadyBegun)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "A gesture is begun.\n"
                  "Client C2 creates an active sync gesture grab\n"
                  "Play events. C2 allows events.\n"
                  "Events for existing gesture should go to C1, events for new gesture should go to C2.\n"
                  "C2 deactivates gesture grab\n"
                  "All events go to C1.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy2);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    GesturePlayBegin();
    ASSERT_EVENT(void, e_c1_begin, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, win, GrabModeSync,
               { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayUpdate();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayEnd();
    GesturePlayBegin();
    GesturePlayUpdate();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayEnd();

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XIAsyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c1_update, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c2_motion1, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c1_end, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c2_begin2, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c2_update2, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c2_motion2, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c2_end2, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIUngrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, CurrentTime);

    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_EVENT(void, e_c1_begin2, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c1_update2, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c1_end2, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GestureActiveGrab, SyncGrabGetsQueuedEventsAfterAsyncAllow)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates an active gesture grab\n"
                  "Play events.\n"
                  "C2 processes gesture begin only after some events are queued.\n"
                  "C2 allows async processing of events via AllowEvents.\n"
                  "All events go to C2 until the grab is deactivated.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    GrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, win, GrabModeSync,
               { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    GesturePlayBegin();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayUpdate();

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_POINTER_ID, XIAsyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c2_motion, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c2_update, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayEnd();
    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayEnd();
    GesturePlayBegin();

    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c2_begin2, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c2_update2, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c2_end2, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_EVENT(void, e_c2_begin3, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIUngrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, CurrentTime);
    ASSERT_EVENT(void, e_c2_end3, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayEnd();
    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_EVENT(void, e_c1_begin2, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c1_update2, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c1_end2, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GestureActiveGrab, SyncGrabGetsQueuedEventsAfterUngrabDuringFreezeBeforeEnd)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates an active gesture grab\n"
                  "Play events.\n"
                  "C2 deactivates gesture grab\n"
                  "All events go to C1.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy2);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    GrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, win, GrabModeSync,
               { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayBegin();
    TouchPadDev().PlayRelMotion(1, 0);

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIUngrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, CurrentTime);

    ASSERT_EVENT(void, e_c1_motion1, dpy1, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c1_begin, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c1_motion2, dpy1, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_EVENT(void, e_c1_update, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c1_end2, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GestureActiveGrab, SyncGrabGetsQueuedEventsAfterUngrabDuringFreezeAfterEnd)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates an active gesture grab\n"
                  "Play events.\n"
                  "C2 deactivates gesture grab\n"
                  "All events go to C1.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy2);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    GrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, win, GrabModeSync,
               { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayBegin();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIUngrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, CurrentTime);

    ASSERT_EVENT(void, e_c1_motion1, dpy1, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c1_begin, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c1_motion2, dpy1, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c1_update, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c1_end2, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_P(GestureActiveGrab, SyncGrabUnfreezesWhenClientExits)
{
    XORG_TESTCASE("Client C1 selects for gestures.\n"
                  "Client C2 creates a active gesture grab\n"
                  "Play gesture sequence.\n"
                  "C2 exits. All events including the replayed ones should go to C1.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    GrabXI2Device(dpy2, VIRTUAL_CORE_POINTER_ID, win, GrabModeSync, GrabModeAsync,
                  { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd(), XI_Motion });

    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayBegin();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayUpdate();
    GesturePlayEnd();

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XCloseDisplay(dpy2);

    ASSERT_EVENT(void, e_c1_motion1, dpy1, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c1_begin, dpy1, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c1_motion2, dpy1, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, e_c1_update, dpy1, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, e_c1_end2, dpy1, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
}

TEST_P(GestureActiveGrab, AsyncGrabKeyboardSync)
{
    XORG_TESTCASE("C2 creates an async gesture grab (keyboard mode sync) on window\n"
                  "Generate gesture begin event\n"
                  "Generate keyboard events\n"
                  "Generate gesture end event\n"
                  "Allow keyboard events.\n"
                  "Make sure all events arrive to client C2.\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = DefaultRootWindow(dpy1);

    SelectXI2Events(dpy1, VIRTUAL_CORE_KEYBOARD_ID, win,
                    { XI_KeyPress, XI_KeyRelease });
    SelectXI2Events(dpy1, VIRTUAL_CORE_POINTER_ID, win,
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    SelectXI2Events(dpy2, VIRTUAL_CORE_KEYBOARD_ID, win, { XI_KeyPress, XI_KeyRelease });
    GrabXI2Device(dpy2, VIRTUAL_CORE_POINTER_ID, win, GrabModeAsync, GrabModeSync,
                  { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GesturePlayBegin();
    KeyboardDev().PlayKeyDown(KEY_A);
    KeyboardDev().PlayKeyUp(KEY_A);
    GesturePlayEnd();

    ASSERT_EVENT(void, e_c2_begin, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, e_c2_end, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowEvents(dpy2, VIRTUAL_CORE_KEYBOARD_ID, XIAsyncDevice, CurrentTime);

    ASSERT_EVENT(void, e_c1_press, dpy1, GenericEvent, xi2_opcode, XI_KeyPress);
    ASSERT_EVENT(void, e_c1_release, dpy1, GenericEvent, xi2_opcode, XI_KeyRelease);
    ASSERT_EVENT(void, e_c2_press, dpy2, GenericEvent, xi2_opcode, XI_KeyPress);
    ASSERT_EVENT(void, e_c2_release, dpy2, GenericEvent, xi2_opcode, XI_KeyRelease);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

INSTANTIATE_TEST_CASE_P(, GestureActiveGrab,
                        ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin));

#endif /* HAVE_XI24 */
