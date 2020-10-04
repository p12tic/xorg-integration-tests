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

#include <stdexcept>
#include <tuple>

#include "helpers.h"
#include "gestures-common.h"

#if HAVE_XI24

class GestureEventTest : public GestureTypesTest {};

TEST_P(GestureEventTest, GestureEventFlags)
{
    XORG_TESTCASE("Register for gesture events on root window.\n"
                  "Trigger gesture begin/cancel\n"
                  "Verify only gesture flags are set on gesture events\n");

    ::Display *dpy = Display();
    SelectXI2Events(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy),
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });

    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayCancel();

    if (IsPinch()) {
        ASSERT_EVENT(XIGesturePinchEvent, ebegin, dpy, GenericEvent, xi2_opcode, GetXIGestureBegin());
        ASSERT_EVENT(XIGesturePinchEvent, eupdate, dpy, GenericEvent, xi2_opcode, GetXIGestureUpdate());
        ASSERT_EVENT(XIGesturePinchEvent, ecancel, dpy, GenericEvent, xi2_opcode, GetXIGestureEnd());
        ASSERT_EQ(ebegin->flags, 0);
        ASSERT_EQ(eupdate->flags, 0);
        ASSERT_EQ(ecancel->flags, XIGesturePinchEventCancelled);
    } else {
        ASSERT_EVENT(XIGestureSwipeEvent, ebegin, dpy, GenericEvent, xi2_opcode, GetXIGestureBegin());
        ASSERT_EVENT(XIGestureSwipeEvent, eupdate, dpy, GenericEvent, xi2_opcode, GetXIGestureUpdate());
        ASSERT_EVENT(XIGestureSwipeEvent, ecancel, dpy, GenericEvent, xi2_opcode, GetXIGestureEnd());
        ASSERT_EQ(ebegin->flags, 0);
        ASSERT_EQ(eupdate->flags, 0);
        ASSERT_EQ(ecancel->flags, XIGestureSwipeEventCancelled);
    }
    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_P(GestureEventTest, DisableDeviceEndGestures)
{
    XORG_TESTCASE("Register for gesture events on root window.\n"
                  "Trigger gesture begin.\n"
                  "Disable the gesture device.\n"
                  "Ensure a GestureFooEnd is sent to that gesture.\n");

    ::Display *dpy = Display();
    SelectXI2Events(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy),
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });
    GesturePlayBegin();
    ASSERT_EVENT(void, ebegin, dpy, GenericEvent, xi2_opcode, GetXIGestureBegin());

    int deviceid = 0;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--touchpad-device--", &deviceid), 1);

    XDevice *xdevice = XOpenDevice(dpy, deviceid);
    ASSERT_TRUE(xdevice != NULL);

    XDeviceEnableControl enable_control;
    enable_control.enable = false;
    XDeviceControl *control = reinterpret_cast<XDeviceControl*>(&enable_control);

    ASSERT_TRUE(NoEventPending(dpy));
    ASSERT_EQ(XChangeDeviceControl(Display(), xdevice, DEVICE_ENABLE, control), Success);
    XCloseDevice(Display(), xdevice);
    XFlush(Display());

    ASSERT_EVENT(void, eend, dpy, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy));
}

INSTANTIATE_TEST_CASE_P(, GestureEventTest,
                        ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin));

class GestureSelectDevicesTest : public GestureTest,
                                 public ::testing::WithParamInterface<std::tuple<int, int, int>> {
public:
    GestureSelectDevicesTest() {
        SetIsPinch(std::get<0>(GetParam()) == XI_GesturePinchBegin);
    }
};

TEST_P(GestureSelectDevicesTest, GestureSelectionDeviceConflicts)
{
    XORG_TESTCASE("Client C1 selects for gesture events on a device.\n"
                  "Client C2 selects for gesture events, expected behavior:\n"
                  "- if C2 selects on the same specific device as A, generate an error.\n"
                  "- if C1 has XIAll(Master)Devices and B selects the same, generate an error.\n"
                  "- if C1 has XIAllDevices and B selects XIAllMasterDevices, allow.\n"
                  "- if C1 has XIAll(Master)Device and B selects a specific device, allow.\n"
                  "Same results with A and B swapped.\n");

    ::Display *dpy = Display();
    ::Display *dpy2 = NewClient();

    int client1_deviceid = std::get<1>(GetParam());
    int client2_deviceid = std::get<2>(GetParam());

    SelectXI2Events(dpy, client1_deviceid, DefaultRootWindow(dpy),
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });
    SetErrorTrap(dpy);
    SelectXI2Events(dpy2, client2_deviceid, DefaultRootWindow(dpy2),
                    { GetXIGestureBegin(), GetXIGestureUpdate(), GetXIGestureEnd() });
    XSync(dpy, False);
    bool received_error = (ReleaseErrorTrap(dpy) != NULL);

    bool want_error = false;

    #define GetMask(deviceid1, deviceid2) ((deviceid1) | (deviceid2) << 8)

    switch (GetMask(client1_deviceid, client2_deviceid)) {
        /* C1 on XIAllDevices */
        case GetMask(XIAllDevices, XIAllDevices):
            want_error = true;
            break;
        case GetMask(XIAllDevices, XIAllMasterDevices):
            want_error = false; /* XXX: Really? */
            break;
        case GetMask(XIAllDevices, VIRTUAL_CORE_POINTER_ID):
            want_error = false;
            break;

        /* C1 on XIAllMasterDevices */
        case GetMask(XIAllMasterDevices, XIAllDevices):
            want_error = false; /* XXX: really? */
            break;
        case GetMask(XIAllMasterDevices, XIAllMasterDevices):
            want_error = true;
            break;
        case GetMask(XIAllMasterDevices, VIRTUAL_CORE_POINTER_ID):
            want_error = false;
            break;

        /* C1 on VCP */
        case GetMask(VIRTUAL_CORE_POINTER_ID, XIAllDevices):
            want_error = false;
            break;
        case GetMask(VIRTUAL_CORE_POINTER_ID, XIAllMasterDevices):
            want_error = false;
            break;
        case GetMask(VIRTUAL_CORE_POINTER_ID, VIRTUAL_CORE_POINTER_ID):
            want_error = true;
            break;
        default:
            FAIL();
            break;
    }
    #undef GetMask

    if (want_error != received_error) {
        ADD_FAILURE() << "Event selection failed\n"
                     "  Client C1 selected on " << DeviceIDToString(client1_deviceid) << "\n"
                     "  Client C2 selected on " << DeviceIDToString(client2_deviceid) << "\n"
                     "  Expected an error? " << want_error << "\n"
                     "  Received an error? " << received_error;
    }
    ASSERT_TRUE(NoEventPending(dpy));
    ASSERT_TRUE(NoEventPending(dpy2));
}

INSTANTIATE_TEST_CASE_P(, GestureSelectDevicesTest,
                        ::testing::Combine(
                            ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin),
                            ::testing::Values(XIAllDevices, XIAllMasterDevices, VIRTUAL_CORE_POINTER_ID),
                            ::testing::Values(XIAllDevices, XIAllMasterDevices, VIRTUAL_CORE_POINTER_ID)));

class GestureSelectPriorityTest : public GestureTest,
                                  public ::testing::WithParamInterface<std::tuple<int, int, int, int, int>> {
public:
    GestureSelectPriorityTest() {
        SetIsPinch(GetEventType() == XI_GesturePinchBegin);
    }
    int GetEventType() const { return std::get<0>(GetParam()); }
    int GetSelect1Type() { return std::get<1>(GetParam()); }
    int GetSelect2Type() { return std::get<2>(GetParam()); }

    std::vector<int> GetSelectEventTypes(int evtype) const {
        if (evtype == XI_GesturePinchBegin) {
            return { XI_GesturePinchBegin, XI_GesturePinchUpdate, XI_GesturePinchEnd };
        } else {
            return { XI_GestureSwipeBegin, XI_GestureSwipeUpdate, XI_GestureSwipeEnd };
        }
    }

    int Window1Depth() const { return std::get<3>(GetParam()); }
    int Window2Depth() const { return std::get<4>(GetParam()); }
};

std::vector<Window> CreateWindowHierarchy(Display *dpy, int depth) {
    Window root = DefaultRootWindow(dpy);

    std::vector<Window> windows;

    windows.push_back(root);

    Window parent = root;
    Window top_parent = None;

    while (depth--) {
        Window win;
        win = XCreateSimpleWindow(dpy, parent, 0, 0,
                                  DisplayWidth(dpy, DefaultScreen(dpy)),
                                  DisplayHeight(dpy, DefaultScreen(dpy)),
                                  0, 0, 0);
        if (top_parent == None)
            XSelectInput(dpy, win, StructureNotifyMask);
        XMapWindow(dpy, win);

        windows.push_back(win);
        parent = win;
    }

    if (windows.size() > 1) {
        XSync(dpy, False);

        if (xorg::testing::XServer::WaitForEventOfType(dpy, MapNotify, -1, -1)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
        } else {
            ADD_FAILURE() << "Failed waiting for Exposure";
        }

        XSelectInput(dpy, windows[1], 0);
    }
    XSync(dpy, True);

    return windows;
}


TEST_P(GestureSelectPriorityTest, SelectionPriorities)
{
    XORG_TESTCASE("Client C1 creates several windows in a hierarchy.\n"
                  "Clients C1 and C2 select for gesture events on different windows.\n"
                  "If a client selects for events on a specific window and there is another\n"
                  "client which selects for the same events deeper in the hierarchy, then\n"
                  "the first client should get no events.\n");

    ::Display *dpy = Display();
    ::Display *dpy2 = NewClient();

    std::vector<Window> windows = CreateWindowHierarchy(dpy, 3);
    Window event1_win = windows[Window1Depth()];
    Window event2_win = windows[Window2Depth()];

    SelectXI2Events(dpy, VIRTUAL_CORE_POINTER_ID, event1_win,
                    GetSelectEventTypes(GetSelect1Type()));
    SetErrorTrap(dpy);
    SelectXI2Events(dpy2, VIRTUAL_CORE_POINTER_ID, event2_win,
                    GetSelectEventTypes(GetSelect2Type()));
    XSync(dpy, False);
    bool received_error = (ReleaseErrorTrap(dpy) != NULL);

    if (GetSelect1Type() == GetSelect2Type() && Window1Depth() == Window2Depth()) {
        ASSERT_TRUE(received_error);
    } else {
        ASSERT_FALSE(received_error);
    }

    bool client1_receive_events = (GetSelect1Type() == GetEventType()) &&
            !(GetSelect1Type() == GetSelect2Type() && Window1Depth() < Window2Depth());
    bool client2_receive_events = (GetSelect2Type() == GetEventType()) &&
            !(GetSelect1Type() == GetSelect2Type() && Window2Depth() <= Window1Depth());

    GesturePlayBegin();
    GesturePlayUpdate();
    GesturePlayEnd();

    std::cout << "ev " << GetEventType() << " sel1 " << GetSelect1Type() << " sel2 " << GetSelect2Type()
              << " wd1 " << Window1Depth() << " wd2 " << Window2Depth() << "\n";
    std::cout << "client1r " << client1_receive_events << " client2r " << client2_receive_events << "\n";

    if (client1_receive_events) {
        ASSERT_EVENT(void, event1, dpy, GenericEvent, xi2_opcode, GetXIGestureBegin());
        ASSERT_EVENT(void, event2, dpy, GenericEvent, xi2_opcode, GetXIGestureUpdate());
        ASSERT_EVENT(void, event3, dpy, GenericEvent, xi2_opcode, GetXIGestureEnd());
    }

    if (client2_receive_events) {
        ASSERT_EVENT(void, event1, dpy2, GenericEvent, xi2_opcode, GetXIGestureBegin());
        ASSERT_EVENT(void, event2, dpy2, GenericEvent, xi2_opcode, GetXIGestureUpdate());
        ASSERT_EVENT(void, event3, dpy2, GenericEvent, xi2_opcode, GetXIGestureEnd());
    }

    ASSERT_TRUE(NoEventPending(dpy));
    ASSERT_TRUE(NoEventPending(dpy2));
}

INSTANTIATE_TEST_CASE_P(, GestureSelectPriorityTest,
                        ::testing::Combine(
                            ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin),
                            ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin),
                            ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin),
                            ::testing::Values(0, 1, 2),
                            ::testing::Values(0, 1, 2)));

class GestureDeviceChangeSameDeviceTest : public GestureTypesTest {};

TEST_P(GestureDeviceChangeSameDeviceTest, SendsDeviceChangedEventWhenSourceDeviceChanges)
{
    XORG_TESTCASE("Register for gesture and motion events on root window.\n"
                  "Trigger mouse and gesture sequences the same device concurrently.\n"
                  "Expect no DeviceChangedEvent\n");

    ::Display *dpy = Display();
    SelectXI2Events(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy),
                    { XI_GesturePinchBegin, XI_GesturePinchUpdate, XI_GesturePinchEnd,
                      XI_GestureSwipeBegin, XI_GestureSwipeUpdate, XI_GestureSwipeEnd,
                      XI_DeviceChanged, XI_Motion });

    GesturePlayBegin();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayUpdate();
    TouchPadDev().PlayRelMotion(1, 0);
    GesturePlayEnd();

    ASSERT_EVENT(void, edcce, dpy, GenericEvent, xi2_opcode, XI_DeviceChanged);
    ASSERT_EVENT(void, ebegin1, dpy, GenericEvent, xi2_opcode, GetXIGestureBegin());
    ASSERT_EVENT(void, emotion1, dpy, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, eupdate1, dpy, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    ASSERT_EVENT(void, emotion2, dpy, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(void, eend1, dpy, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy));
}

INSTANTIATE_TEST_CASE_P(, GestureDeviceChangeSameDeviceTest,
                        ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin));

class GestureMultipleDevicesTest : public GestureTest,
                                   public ::testing::WithParamInterface<std::tuple<int, int>> {
public:
    xorg::testing::emulated::Device& TouchPad2Dev() { return Dev(3); }
    bool IsPinch2() const { return std::get<1>(GetParam()); }

    GestureMultipleDevicesTest() {
        SetIsPinch(std::get<0>(GetParam()) == XI_GesturePinchBegin);
    }

    void SetUp() override {
        AddDevice(xorg::testing::emulated::DeviceType::POINTER_GESTURE);
        AddDevice(xorg::testing::emulated::DeviceType::TOUCH);
        AddDevice(xorg::testing::emulated::DeviceType::KEYBOARD);
        AddDevice(xorg::testing::emulated::DeviceType::POINTER_GESTURE);

        xi2_major_minimum = 2;
        xi2_minor_minimum = 4;

        XITServerInputTest::SetUp();
    }

    void AssertEventWithDeltaX(::Display* dpy, double delta_x, int event_type) {
        if (event_type == XI_GesturePinchBegin || event_type == XI_GesturePinchUpdate ||
            event_type == XI_GesturePinchEnd) {
            ASSERT_EVENT(XIGesturePinchEvent, event, dpy, GenericEvent, xi2_opcode, event_type);
            ASSERT_EQ(event->delta_x, delta_x);
        } else {
            ASSERT_EVENT(XIGestureSwipeEvent, event, dpy, GenericEvent, xi2_opcode, event_type);
            ASSERT_EQ(event->delta_x, delta_x);
        }
    }

    void AssertDeviceChangedEvent(::Display* dpy, int device_id) {
        ASSERT_EVENT(XIDeviceChangedEvent, event, dpy, GenericEvent, xi2_opcode, XI_DeviceChanged);
        ASSERT_EQ(event->deviceid, VIRTUAL_CORE_POINTER_ID);
        ASSERT_EQ(event->sourceid, device_id);
        ASSERT_EQ(event->reason, XISlaveSwitch);
    }

    void SetUpConfigAndLog() override {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("test", "--touchpad-device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n" +
                               TouchPadDev().GetOptions());
        config.AddInputSection("test", "--touch-device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n" +
                               TouchDev().GetOptions());
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("test", "--kbd-device--",
                               "Option \"CoreKeyboard\" \"on\"\n" +
                               KeyboardDev().GetOptions());
        config.AddInputSection("test", "--touchpad2-device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n" +
                               TouchPad2Dev().GetOptions());
        config.WriteConfig();
    }
};

class GestureDeviceChangeMultipleGesturesDeviceTest : public GestureMultipleDevicesTest {};

TEST_P(GestureDeviceChangeMultipleGesturesDeviceTest, IgnoresSimulataneousGestures)
{
    XORG_TESTCASE("Register for gesture events on root window.\n"
                  "Trigger two concurrent gestures from two devices simultaneously\n"
                  "Expect that the second gesture will be ignored.\n");

    ::Display *dpy = Display();
    SelectXI2Events(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy),
                    { XI_GesturePinchBegin, XI_GesturePinchUpdate, XI_GesturePinchEnd,
                      XI_GestureSwipeBegin, XI_GestureSwipeUpdate, XI_GestureSwipeEnd,
                      XI_DeviceChanged });

    if (IsPinch()) {
        TouchPadDev().PlayGesturePinchBegin(3, 10, 0, 0, 0, 1.0, 0);
    } else {
        TouchPadDev().PlayGestureSwipeBegin(3, 10, 0, 0, 0);
    }
    if (IsPinch2()) {
        TouchPad2Dev().PlayGesturePinchBegin(3, 20, 0, 0, 0, 1.0, 0);
    } else {
        TouchPad2Dev().PlayGestureSwipeBegin(3, 20, 0, 0, 0);
    }
    if (IsPinch()) {
        TouchPadDev().PlayGesturePinchUpdate(3, 10, 1, 0, 0, 1.0, 0);
    } else {
        TouchPadDev().PlayGestureSwipeUpdate(3, 10, 1, 0, 0);
    }
    if (IsPinch2()) {
        TouchPad2Dev().PlayGesturePinchUpdate(3, 20, 1, 0, 0, 1.0, 0);
    } else {
        TouchPad2Dev().PlayGestureSwipeUpdate(3, 20, 1, 0, 0);
    }
    if (IsPinch()) {
        TouchPadDev().PlayGesturePinchEnd(3, 10, 1, 1, 1, 1.0, 0);
    } else {
        TouchPadDev().PlayGestureSwipeEnd(3, 10, 1, 1, 1);
    }
    if (IsPinch2()) {
        TouchPad2Dev().PlayGesturePinchEnd(3, 20, 1, 1, 1, 1.0, 0);
    } else {
        TouchPad2Dev().PlayGestureSwipeEnd(3, 20, 1, 1, 1);
    }

    int deviceid1 = 0;
    int deviceid2 = 0;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--touchpad-device--", &deviceid1), 1);
    ASSERT_EQ(FindInputDeviceByName(dpy, "--touchpad2-device--", &deviceid2), 1);

    // FIXME: we ignore simultaneous gestures on multiple devices, but the DCCE events are generated
    // during event submission, so we get back to back DCCE events that change to and from a
    // slave device.
    AssertDeviceChangedEvent(dpy, deviceid1);
    AssertEventWithDeltaX(dpy, 10.0, GetXIGestureBegin());
    AssertDeviceChangedEvent(dpy, deviceid2);
    AssertDeviceChangedEvent(dpy, deviceid1);
    AssertEventWithDeltaX(dpy, 10.0, GetXIGestureUpdate());
    AssertDeviceChangedEvent(dpy, deviceid2);
    AssertDeviceChangedEvent(dpy, deviceid1);
    AssertEventWithDeltaX(dpy, 10.0, GetXIGestureEnd());
    AssertDeviceChangedEvent(dpy, deviceid2);
    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_P(GestureDeviceChangeMultipleGesturesDeviceTest, IgnoresSimulataneousGesturesMultiple)
{
    XORG_TESTCASE("Register for gesture events on root window.\n"
                  "Trigger several concurrent gestures from two devices simultaneously\n"
                  "Expect that the second gesture will be ignored.\n");

    ::Display *dpy = Display();
    SelectXI2Events(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy),
                    { XI_GesturePinchBegin, XI_GesturePinchUpdate, XI_GesturePinchEnd,
                      XI_GestureSwipeBegin, XI_GestureSwipeUpdate, XI_GestureSwipeEnd,
                      XI_DeviceChanged });

    if (IsPinch()) {
        TouchPadDev().PlayGesturePinchBegin(3, 10, 0, 0, 0, 1.0, 0);
    } else {
        TouchPadDev().PlayGestureSwipeBegin(3, 10, 0, 0, 0);
    }
    if (IsPinch2()) {
        TouchPad2Dev().PlayGesturePinchBegin(3, 20, 0, 0, 0, 1.0, 0);
    } else {
        TouchPad2Dev().PlayGestureSwipeBegin(3, 20, 0, 0, 0);
    }
    if (IsPinch()) {
        TouchPadDev().PlayGesturePinchUpdate(3, 10, 1, 0, 0, 1.0, 0);
    } else {
        TouchPadDev().PlayGestureSwipeUpdate(3, 10, 1, 0, 0);
    }
    if (IsPinch2()) {
        TouchPad2Dev().PlayGesturePinchUpdate(3, 20, 1, 0, 0, 1.0, 0);
        TouchPad2Dev().PlayGesturePinchEnd(3, 20, 1, 1, 1, 1.0, 0);

        TouchPad2Dev().PlayGesturePinchBegin(4, 20, 0, 0, 0, 1.0, 0);
        TouchPad2Dev().PlayGesturePinchUpdate(4, 20, 1, 0, 0, 1.0, 0);
        TouchPad2Dev().PlayGesturePinchEnd(4, 20, 1, 1, 1, 1.0, 0);

    } else {
        TouchPad2Dev().PlayGestureSwipeUpdate(3, 20, 1, 0, 0);
        TouchPad2Dev().PlayGestureSwipeEnd(3, 20, 1, 1, 1);

        TouchPad2Dev().PlayGestureSwipeBegin(4, 20, 0, 0, 0);
        TouchPad2Dev().PlayGestureSwipeUpdate(4, 20, 1, 0, 0);
        TouchPad2Dev().PlayGestureSwipeEnd(4, 20, 1, 1, 1);
    }
    if (IsPinch()) {
        TouchPadDev().PlayGesturePinchEnd(3, 10, 1, 1, 1, 1.0, 0);
    } else {
        TouchPadDev().PlayGestureSwipeEnd(3, 10, 1, 1, 1);
    }

    int deviceid1 = 0;
    int deviceid2 = 0;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--touchpad-device--", &deviceid1), 1);
    ASSERT_EQ(FindInputDeviceByName(dpy, "--touchpad2-device--", &deviceid2), 1);

    // FIXME: we ignore simultaneous gestures on multiple devices, but the DCCE events are generated
    // during event submission, so we get back to back DCCE events that change to and from a
    // slave device.
    AssertDeviceChangedEvent(dpy, deviceid1);
    AssertEventWithDeltaX(dpy, 10.0, GetXIGestureBegin());
    AssertDeviceChangedEvent(dpy, deviceid2);
    AssertDeviceChangedEvent(dpy, deviceid1);
    AssertEventWithDeltaX(dpy, 10.0, GetXIGestureUpdate());
    AssertDeviceChangedEvent(dpy, deviceid2);
    AssertDeviceChangedEvent(dpy, deviceid1);
    AssertEventWithDeltaX(dpy, 10.0, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_P(GestureDeviceChangeMultipleGesturesDeviceTest,
       SendsDeviceChangedEventWhenSourceDeviceChanges)
{
    XORG_TESTCASE("Register for gesture events on root window.\n"
                  "Trigger gesture sequences from two devices non-concurrently.\n"
                  "Expect a DeviceChangedEvent for the mouse on the VCP\n");

    ::Display *dpy = Display();
    SelectXI2Events(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy),
                    { XI_GesturePinchBegin, XI_GesturePinchUpdate, XI_GesturePinchEnd,
                      XI_GestureSwipeBegin, XI_GestureSwipeUpdate, XI_GestureSwipeEnd,
                      XI_DeviceChanged });

    if (IsPinch()) {
        TouchPadDev().PlayGesturePinchBegin(3, 10, 0, 0, 0, 1.0, 0);
        TouchPadDev().PlayGesturePinchUpdate(3, 10, 1, 0, 0, 1.0, 0);
        TouchPadDev().PlayGesturePinchEnd(3, 10, 1, 1, 1, 1.0, 0);
    } else {
        TouchPadDev().PlayGestureSwipeBegin(3, 10, 0, 0, 0);
        TouchPadDev().PlayGestureSwipeUpdate(3, 10, 1, 0, 0);
        TouchPadDev().PlayGestureSwipeEnd(3, 10, 1, 1, 1);
    }
    if (IsPinch2()) {
        TouchPad2Dev().PlayGesturePinchBegin(3, 20, 0, 0, 0, 1.0, 0);
        TouchPad2Dev().PlayGesturePinchUpdate(3, 20, 1, 0, 0, 1.0, 0);
        TouchPad2Dev().PlayGesturePinchEnd(3, 20, 1, 1, 1, 1.0, 0);
    } else {
        TouchPad2Dev().PlayGestureSwipeBegin(3, 20, 0, 0, 0);
        TouchPad2Dev().PlayGestureSwipeUpdate(3, 20, 1, 0, 0);
        TouchPad2Dev().PlayGestureSwipeEnd(3, 20, 1, 1, 1);
    }

    int deviceid1 = 0, deviceid2 = 0;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--touchpad-device--", &deviceid1), 1);
    ASSERT_EQ(FindInputDeviceByName(dpy, "--touchpad2-device--", &deviceid2), 1);

    AssertDeviceChangedEvent(dpy, deviceid1);
    AssertEventWithDeltaX(dpy, 10.0, GetXIGestureBegin());
    AssertEventWithDeltaX(dpy, 10.0, GetXIGestureUpdate());
    AssertEventWithDeltaX(dpy, 10.0, GetXIGestureEnd());
    AssertDeviceChangedEvent(dpy, deviceid2);
    if (IsPinch2()) {
        AssertEventWithDeltaX(dpy, 20.0, XI_GesturePinchBegin);
        AssertEventWithDeltaX(dpy, 20.0, XI_GesturePinchUpdate);
        AssertEventWithDeltaX(dpy, 20.0, XI_GesturePinchEnd);
    } else {
        AssertEventWithDeltaX(dpy, 20.0, XI_GestureSwipeBegin);
        AssertEventWithDeltaX(dpy, 20.0, XI_GestureSwipeUpdate);
        AssertEventWithDeltaX(dpy, 20.0, XI_GestureSwipeEnd);
    }
    ASSERT_TRUE(NoEventPending(dpy));
}

INSTANTIATE_TEST_CASE_P(, GestureDeviceChangeMultipleGesturesDeviceTest,
                        ::testing::Combine(
                            ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin),
                            ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin)));

class GestureDeviceChangeMultiplePointerDeviceTest : public GestureMultipleDevicesTest {};

TEST_P(GestureDeviceChangeMultiplePointerDeviceTest, DeviceChangedEventSimultaneousPointerEvents)
{
    XORG_TESTCASE("Register for gesture and motion events on root window.\n"
                  "Trigger concurrent gesture and pointer events from two devices simultaneously\n"
                  "Expect all input and device changed events to arrive.\n");

    ::Display *dpy = Display();
    SelectXI2Events(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy),
                    { XI_GesturePinchBegin, XI_GesturePinchUpdate, XI_GesturePinchEnd,
                      XI_GestureSwipeBegin, XI_GestureSwipeUpdate, XI_GestureSwipeEnd,
                      XI_DeviceChanged, XI_Motion });

    GesturePlayBegin();
    TouchPad2Dev().PlayRelMotion(1, 0);
    GesturePlayUpdate();
    TouchPad2Dev().PlayRelMotion(1, 0);
    GesturePlayEnd();

    int deviceid1 = 0;
    int deviceid2 = 0;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--touchpad-device--", &deviceid1), 1);
    ASSERT_EQ(FindInputDeviceByName(dpy, "--touchpad2-device--", &deviceid2), 1);

    AssertDeviceChangedEvent(dpy, deviceid1);
    ASSERT_EVENT(void, e_begin, dpy, GenericEvent, xi2_opcode, GetXIGestureBegin());
    AssertDeviceChangedEvent(dpy, deviceid2);
    ASSERT_EVENT(void, e_motion1, dpy, GenericEvent, xi2_opcode, XI_Motion);
    AssertDeviceChangedEvent(dpy, deviceid1);
    ASSERT_EVENT(void, e_update, dpy, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    AssertDeviceChangedEvent(dpy, deviceid2);
    ASSERT_EVENT(void, e_motion2, dpy, GenericEvent, xi2_opcode, XI_Motion);
    AssertDeviceChangedEvent(dpy, deviceid1);
    ASSERT_EVENT(void, e_end, dpy, GenericEvent, xi2_opcode, GetXIGestureEnd());
    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_P(GestureDeviceChangeMultiplePointerDeviceTest, DeviceChangedEventSimultaneousTouchEvents)
{
    XORG_TESTCASE("Register for gesture and motion events on root window.\n"
                  "Trigger concurrent gesture and pointer events from two devices simultaneously\n"
                  "Expect all input and device changed events to arrive.\n");

    ::Display *dpy = Display();
    SelectXI2Events(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy),
                    { XI_GesturePinchBegin, XI_GesturePinchUpdate, XI_GesturePinchEnd,
                      XI_GestureSwipeBegin, XI_GestureSwipeUpdate, XI_GestureSwipeEnd,
                      XI_DeviceChanged, XI_TouchBegin, XI_TouchUpdate, XI_TouchEnd });

    GesturePlayBegin();
    TouchDev().PlayTouchBegin(100, 100, 1);
    GesturePlayUpdate();
    TouchDev().PlayTouchUpdate(100, 100, 1);
    GesturePlayEnd();
    TouchDev().PlayTouchEnd(100, 100, 1);

    int deviceid1 = 0;
    int deviceid2 = 0;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--touchpad-device--", &deviceid1), 1);
    ASSERT_EQ(FindInputDeviceByName(dpy, "--touch-device--", &deviceid2), 1);

    AssertDeviceChangedEvent(dpy, deviceid1);
    ASSERT_EVENT(void, e_begin, dpy, GenericEvent, xi2_opcode, GetXIGestureBegin());
    AssertDeviceChangedEvent(dpy, deviceid2);
    ASSERT_EVENT(void, e_touch_begin, dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
    AssertDeviceChangedEvent(dpy, deviceid1);
    ASSERT_EVENT(void, e_update, dpy, GenericEvent, xi2_opcode, GetXIGestureUpdate());
    AssertDeviceChangedEvent(dpy, deviceid2);
    ASSERT_EVENT(void, e_touch_update, dpy, GenericEvent, xi2_opcode, XI_TouchUpdate);
    AssertDeviceChangedEvent(dpy, deviceid1);
    ASSERT_EVENT(void, e_end, dpy, GenericEvent, xi2_opcode, GetXIGestureEnd());
    AssertDeviceChangedEvent(dpy, deviceid2);
    ASSERT_EVENT(void, e_touch_end, dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(NoEventPending(dpy));
}

INSTANTIATE_TEST_CASE_P(, GestureDeviceChangeMultiplePointerDeviceTest,
                        ::testing::Combine(
                            ::testing::Values(XI_GesturePinchBegin, XI_GestureSwipeBegin),
                            ::testing::Values(XI_GesturePinchBegin)));

#endif
