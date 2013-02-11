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
#include <tr1/tuple>

#include "helpers.h"
#include "device-interface.h"
#include "xit-server-input-test.h"

#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XI2.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

class TouchTest : public XITServerInputTest,
                  public DeviceInterface {
protected:
    virtual void SetUp() {
        SetDevice("tablets/N-Trig-MultiTouch.desc");

        xi2_major_minimum = 2;
        xi2_minor_minimum = 2;

        XITServerInputTest::SetUp();
    }

    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputClass("grab device", "MatchIsTouchscreen \"on\"",
                             "Option \"GrabDevice\" \"on\"");
        config.SetAutoAddDevices(true);
        config.WriteConfig();
    }

    virtual void TouchBegin(int x, int y) {
        dev->PlayOne(EV_KEY, BTN_TOUCH, 1);
        TouchUpdate(x, y);
    }

    virtual void TouchUpdate(int x, int y) {
        dev->PlayOne(EV_ABS, ABS_X, x);
        dev->PlayOne(EV_ABS, ABS_Y, y);
        dev->PlayOne(EV_ABS, ABS_MT_POSITION_X, x);
        dev->PlayOne(EV_ABS, ABS_MT_POSITION_Y, y);
        /* same values as the recordings file */
        dev->PlayOne(EV_ABS, ABS_MT_ORIENTATION, 0);
        dev->PlayOne(EV_ABS, ABS_MT_TOUCH_MAJOR, 468);
        dev->PlayOne(EV_ABS, ABS_MT_TOUCH_MINOR, 306);
        dev->PlayOne(EV_SYN, SYN_MT_REPORT, 0);
        dev->PlayOne(EV_SYN, SYN_REPORT, 0);
    }

    virtual void TouchEnd() {
        dev->PlayOne(EV_KEY, BTN_TOUCH, 0);
        dev->PlayOne(EV_SYN, SYN_REPORT, 0);
    }
};

/**
 * A test fixture for testing touch across XInput 2.x extension versions.
 *
 * @tparam The XInput 2.x minor version
 */
class TouchTestXI2Version : public TouchTest,
                            public ::testing::WithParamInterface<int> {
protected:
    virtual void RequireXI2(int major, int minor, int *maj_ret, int *min_ret)
    {
        XITServerInputTest::RequireXI2(2, GetParam());
    }
};

TEST_P(TouchTestXI2Version, XITouchscreenPointerEmulation)
{
    XORG_TESTCASE("Register for button and motion events.\n"
                  "Create a touch and a pointer device.\n"
                  "Moving the mouse must have a zero button mask.\n"
                  "Create a touch point on the screen.\n"
                  "Moving the mouse now must have button 1 set.\n"
                  "Release the touch point.\n"
                  "Moving the mouse now must have a zero button mask.\n");

    ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), "N-Trig MultiTouch"));

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_Motion);
    mask.mask = reinterpret_cast<unsigned char*>(calloc(mask.mask_len, 1));
    XISetMask(mask.mask, XI_ButtonPress);
    XISetMask(mask.mask, XI_ButtonRelease);
    XISetMask(mask.mask, XI_Motion);

    XISelectEvents(Display(), DefaultRootWindow(Display()), &mask, 1);

    free(mask.mask);

    XFlush(Display());

    std::auto_ptr<xorg::testing::evemu::Device> mouse_device;
    try {
      mouse_device = std::auto_ptr<xorg::testing::evemu::Device>(
          new xorg::testing::evemu::Device(
              RECORDINGS_DIR "mice/PIXART-USB-OPTICAL-MOUSE.desc")
          );
    } catch (std::runtime_error &error) {
      std::cerr << "Failed to create evemu device, skipping test.\n";
      return;
    }

    ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), "PIXART USB OPTICAL MOUSE"));

    XEvent event;
    XGenericEventCookie *xcookie;
    XIDeviceEvent *device_event;

    /* Move the mouse, check that the button is not pressed. */
    mouse_device->PlayOne(EV_REL, ABS_X, -1, 1);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_Motion));

    ASSERT_EQ(Success, XNextEvent(Display(), &event));
    xcookie = &event.xcookie;
    ASSERT_TRUE(XGetEventData(Display(), xcookie));

    device_event = reinterpret_cast<XIDeviceEvent*>(xcookie->data);
    ASSERT_FALSE(XIMaskIsSet(device_event->buttons.mask, 1));
    XFreeEventData(Display(), xcookie);


    /* Touch the screen, wait for press event */
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonPress));

    ASSERT_EQ(Success, XNextEvent(Display(), &event));


    /* Move the mouse again, button 1 should now be pressed. */
    mouse_device->PlayOne(EV_REL, ABS_X, -1, 1);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_Motion));

    ASSERT_EQ(Success, XNextEvent(Display(), &event));
    xcookie = &event.xcookie;
    ASSERT_TRUE(XGetEventData(Display(), xcookie));

    device_event = reinterpret_cast<XIDeviceEvent*>(xcookie->data);
    ASSERT_TRUE(XIMaskIsSet(device_event->buttons.mask, 1));
    XFreeEventData(Display(), xcookie);


    /* Release the screen, wait for release event */
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonRelease));

    ASSERT_EQ(Success, XNextEvent(Display(), &event));


    /* Move the mouse again, button 1 should now be released. */
    mouse_device->PlayOne(EV_REL, ABS_X, -1, 1);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_Motion));

    ASSERT_EQ(Success, XNextEvent(Display(), &event));
    xcookie = &event.xcookie;
    ASSERT_TRUE(XGetEventData(Display(), xcookie));

    device_event = reinterpret_cast<XIDeviceEvent*>(xcookie->data);
    ASSERT_FALSE(XIMaskIsSet(device_event->buttons.mask, 1));
    XFreeEventData(Display(), xcookie);
}

TEST_P(TouchTestXI2Version, EmulatedButtonMaskOnTouchBeginEndCore)
{
    XORG_TESTCASE("Select for core Motion and ButtonPress/Release events on the root window.\n"
                  "Create a pointer-emulating touch event.\n"
                  "Expect a Motion event before the button press.\n"
                  "The button mask in the motion event must be 0.\n"
                  "The button mask in the button press event must be 0.\n"
                  "The button mask in the button release event must be set for button 1.");

    XSelectInput(Display(), DefaultRootWindow(Display()),
                 PointerMotionMask|ButtonPressMask|ButtonReleaseMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           MotionNotify,
                                                           -1, -1));
    XEvent ev;
    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.type, MotionNotify);
    EXPECT_EQ(ev.xmotion.state, 0U);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           ButtonPress,
                                                           -1, -1));
    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.type, ButtonPress);
    EXPECT_EQ(ev.xbutton.state, 0U);
    EXPECT_EQ(ev.xbutton.button, 1U);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           ButtonRelease,
                                                           -1, -1));
    while (ev.type != ButtonRelease)
        XNextEvent(Display(), &ev);

    ASSERT_EQ(ev.type, ButtonRelease);
    EXPECT_EQ(ev.xbutton.state, (unsigned int)Button1Mask);
    EXPECT_EQ(ev.xbutton.button, 1U) << "ButtonRelease must have button 1 mask down";
}

TEST_P(TouchTestXI2Version, EmulatedButton1MotionMaskOnTouch)
{
    XORG_TESTCASE("Select for core Pointer1Motion mask on the root window.\n"
                  "Create a pointer-emulating touch event.\n"
                  "Expect a motion event with the button mask 1.\n")

    XSelectInput(Display(), DefaultRootWindow(Display()), Button1MotionMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           MotionNotify,
                                                           -1, -1));
    XEvent ev;
    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.type, MotionNotify);
    EXPECT_EQ(ev.xmotion.state, (unsigned int)Button1Mask);
}

TEST_P(TouchTestXI2Version, EmulatedButtonMaskOnTouchBeginEndXI2)
{
    XORG_TESTCASE("Select for XI_Motion and XI_ButtonPress/Release events on the root window.\n"
                  "Create a pointer-emulating touch event.\n"
                  "Expect a Motion event before the button press.\n"
                  "The button mask in the motion event must be 0.\n"
                  "The button mask in the button press event must be 0.\n"
                  "The button mask in the button release event must be set for button 1.");

    XIEventMask mask;
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask_len = XIMaskLen(XI_Motion);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_Motion);
    XISetMask(mask.mask, XI_ButtonPress);
    XISetMask(mask.mask, XI_ButtonRelease);

    ASSERT_EQ(Success,
              XISelectEvents(Display(), DefaultRootWindow(Display()), &mask, 1));
    XSync(Display(), False);
    delete[] mask.mask;

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_Motion));
    XEvent ev;
    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.type, GenericEvent);
    ASSERT_EQ(ev.xcookie.extension, xi2_opcode);
    ASSERT_EQ(ev.xcookie.evtype, XI_Motion);
    ASSERT_TRUE(XGetEventData(Display(), &ev.xcookie));

    XIDeviceEvent *motion = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);
    for (int i = 0; i < motion->buttons.mask_len * 8; i++)
        EXPECT_FALSE(XIMaskIsSet(motion->buttons.mask, i)) << "mask down for button " << i;

    XFreeEventData(Display(), &ev.xcookie);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonPress));
    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.type, GenericEvent);
    ASSERT_EQ(ev.xcookie.extension, xi2_opcode);
    ASSERT_EQ(ev.xcookie.evtype, XI_ButtonPress);
    ASSERT_TRUE(XGetEventData(Display(), &ev.xcookie));

    XIDeviceEvent *press = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);
    for (int i = 0; i < press->buttons.mask_len * 8; i++)
        EXPECT_FALSE(XIMaskIsSet(press->buttons.mask, i)) << "mask down for button " << i;

    XFreeEventData(Display(), &ev.xcookie);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonRelease));
    while(ev.xcookie.evtype != XI_ButtonRelease)
        XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.type, GenericEvent);
    ASSERT_EQ(ev.xcookie.extension, xi2_opcode);
    ASSERT_EQ(ev.xcookie.evtype, XI_ButtonRelease);
    ASSERT_TRUE(XGetEventData(Display(), &ev.xcookie));

    XIDeviceEvent *release = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);
    EXPECT_GT(release->buttons.mask_len, 0);
    EXPECT_TRUE(XIMaskIsSet(release->buttons.mask, 1)) << "ButtonRelease must have button 1 down";
    for (int i = 0; i < release->buttons.mask_len * 8; i++) {
        if (i == 1)
            continue;
        EXPECT_FALSE(XIMaskIsSet(release->buttons.mask, i)) << "mask down for button " << i;
    }

    XFreeEventData(Display(), &ev.xcookie);
}

TEST_P(TouchTestXI2Version, XIQueryPointerTouchscreen)
{
    XORG_TESTCASE("Create a touch device, create a touch point from that device\n"
                  "XIQueryPointer() should return:\n"
                  " - button 1 state down for XI 2.0 and 2.1 clients\n"
                  " - button 1 state up for XI 2.2+ clients");

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_ButtonPress);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_ButtonPress);

    ASSERT_EQ(Success,
              XISelectEvents(Display(), DefaultRootWindow(Display()), &mask,
                             1));

    delete[] mask.mask;

    XFlush(Display());

    ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), "N-Trig MultiTouch"));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonPress));

    XEvent event;
    ASSERT_EQ(Success, XNextEvent(Display(), &event));

    XGenericEventCookie *xcookie = &event.xcookie;
    ASSERT_TRUE(XGetEventData(Display(), xcookie));

    XIDeviceEvent *device_event =
        reinterpret_cast<XIDeviceEvent*>(xcookie->data);

    Window root;
    Window child;
    double root_x;
    double root_y;
    double win_x;
    double win_y;
    XIButtonState buttons;
    XIModifierState modifiers;
    XIGroupState group;
    ASSERT_TRUE(XIQueryPointer(Display(), device_event->deviceid,
                               DefaultRootWindow(Display()), &root, &child,
                               &root_x, &root_y, &win_x, &win_y, &buttons,
                               &modifiers, &group));

    /* Test if button 1 is pressed */
    ASSERT_GE(buttons.mask_len, XIMaskLen(2));
    if (GetParam() < 2)
        EXPECT_TRUE(XIMaskIsSet(buttons.mask, 1));
    else
        EXPECT_FALSE(XIMaskIsSet(buttons.mask, 1));

    XFreeEventData(Display(), xcookie);
}

#ifdef HAVE_XI22
class TouchDeviceTest : public TouchTest {};

TEST_F(TouchDeviceTest, DisableDeviceEndTouches)
{
    XORG_TESTCASE("Register for touch events.\n"
                  "Create a touch device, create a touch point.\n"
                  "Disable the touch device.\n"
                  "Ensure a TouchEnd is sent for that touch point\n");

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    ASSERT_EQ(Success,
              XISelectEvents(Display(), DefaultRootWindow(Display()), &mask,
                             1));

    delete[] mask.mask;

    XFlush(Display());

    ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), "N-Trig MultiTouch"));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_TouchBegin));

    XEvent event;
    ASSERT_EQ(Success, XNextEvent(Display(), &event));

    XGenericEventCookie *xcookie = &event.xcookie;
    ASSERT_TRUE(XGetEventData(Display(), xcookie));

    XIDeviceEvent *device_event =
        reinterpret_cast<XIDeviceEvent*>(xcookie->data);

    XDevice *xdevice = XOpenDevice(Display(), device_event->sourceid);
    XFreeEventData(Display(), xcookie);
    ASSERT_TRUE(xdevice != NULL);

    XDeviceEnableControl enable_control;
    enable_control.enable = false;
    XDeviceControl *control =
        reinterpret_cast<XDeviceControl*>(&enable_control);
    ASSERT_EQ(XChangeDeviceControl(Display(), xdevice, DEVICE_ENABLE, control),
              Success);
    XCloseDevice(Display(), xdevice);
    XFlush(Display());

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_TouchEnd));
}

/**
 * Test class for testing XISelectEvents on touch masks
 *
 * @tparam The device ID
 */
class XISelectEventsTouchTest : public TouchTest,
                                public ::testing::WithParamInterface<std::tr1::tuple<int, int> >
{
};

TEST_P(XISelectEventsTouchTest, TouchSelectionConflicts)
{
    XORG_TESTCASE("Client A selects for touch events on a device.\n"
                  "Client B selects for touch events, expected behavior:\n"
                  "- if B selects on the same specific device as A, generate an error.\n"
                  "- if A has XIAll(Master)Devices and B selects the same, generate an error.\n"
                  "- if A has XIAllDevices and B selects XIAllMasterDevices, allow.\n"
                  "- if A has XIAll(Master)Device and B selects a specific device, allow.\n"
                  "Same results with A and B swapped");

    unsigned char m[XIMaskLen(XI_TouchEnd)] = {0};
    XIEventMask mask;
    mask.mask = m;
    mask.mask_len = sizeof(m);

    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    std::tr1::tuple<int, int> t = GetParam();
    int clientA_deviceid = std::tr1::get<0>(t);
    int clientB_deviceid = std::tr1::get<1>(t);

    /* client A */
    mask.deviceid = clientA_deviceid;
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy2);

    int major = 2, minor = 2;
    XIQueryVersion(dpy2, &major, &minor);

    XISelectEvents(dpy2, DefaultRootWindow(dpy2), &mask, 1);
    XSync(dpy2, False);

    bool want_error = false, received_error = false;

    mask.deviceid = clientB_deviceid;
    SetErrorTrap(Display());
    XISelectEvents(Display(), DefaultRootWindow(Display()), &mask, 1);
    XSync(Display(), False);
    received_error = (ReleaseErrorTrap(Display()) != NULL);

#define A(mask) (mask)
#define B(mask) (mask << 8)

    switch (clientA_deviceid | clientB_deviceid << 8) {
        /* A on XIAllDevices */
        case A(XIAllDevices) | B(XIAllDevices):
            want_error = true;
            break;
        case A(XIAllDevices) | B(XIAllMasterDevices):
            want_error = false; /* XXX: Really? */
            break;
        case A(XIAllDevices) | B(VIRTUAL_CORE_POINTER_ID):
            want_error = false;
            break;

        /* A on XIAllMasterDevices */
        case A(XIAllMasterDevices) | B(XIAllDevices):
            want_error = false; /* XXX: really? */
            break;
        case A(XIAllMasterDevices) | B(XIAllMasterDevices):
            want_error = true;
            break;
        case A(XIAllMasterDevices) | B(VIRTUAL_CORE_POINTER_ID):
            want_error = false;
            break;

        /* A on VCP */
        case A(VIRTUAL_CORE_POINTER_ID) | B(XIAllDevices):
            want_error = false;
            break;
        case A(VIRTUAL_CORE_POINTER_ID) | B(XIAllMasterDevices):
            want_error = false;
            break;
        case A(VIRTUAL_CORE_POINTER_ID) | B(VIRTUAL_CORE_POINTER_ID):
            want_error = true;
            break;
        default:
            FAIL();
            break;
#undef A
#undef B
    }

    if (want_error != received_error) {
        ADD_FAILURE() << "Event selection failed\n"
                     "  Client A selected on " << DeviceIDToString(clientA_deviceid) << "\n"
                     "  Client B selected on " << DeviceIDToString(clientB_deviceid) << "\n"
                     "  Expected an error? " << want_error << "\n"
                     "  Received an error? " << received_error;
    }
}

INSTANTIATE_TEST_CASE_P(, XISelectEventsTouchTest,
        ::testing::Combine(
            ::testing::Values(XIAllDevices, XIAllMasterDevices, VIRTUAL_CORE_POINTER_ID),
            ::testing::Values(XIAllDevices, XIAllMasterDevices, VIRTUAL_CORE_POINTER_ID)));


TEST_F(TouchTest, TouchEventsButtonState)
{
    XORG_TESTCASE("Select for touch events on the root window.\n"
                  "Create a pointer-emulating touch event.\n"
                  "The button mask in the touch begin, update, and end event must be 0.");

    XIEventMask mask;
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    ASSERT_EQ(Success,
              XISelectEvents(Display(), DefaultRootWindow(Display()), &mask, 1));
    XSync(Display(), False);
    delete[] mask.mask;

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_TouchBegin));

    XEvent ev;
    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.type, GenericEvent);
    ASSERT_EQ(ev.xcookie.extension, xi2_opcode);
    ASSERT_EQ(ev.xcookie.evtype, XI_TouchBegin);
    ASSERT_TRUE(XGetEventData(Display(), &ev.xcookie));

    XIDeviceEvent *begin = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);
    for (int i = 0; i < begin->buttons.mask_len * 8; i++)
        EXPECT_FALSE(XIMaskIsSet(begin->buttons.mask, i)) << "mask down for button " << i;

    XFreeEventData(Display(), &ev.xcookie);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_TouchEnd));

    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.type, GenericEvent);
    ASSERT_EQ(ev.xcookie.extension, xi2_opcode);
    ASSERT_EQ(ev.xcookie.evtype, XI_TouchEnd);
    ASSERT_TRUE(XGetEventData(Display(), &ev.xcookie));

    XIDeviceEvent *end = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);
    EXPECT_GT(end->buttons.mask_len, 0);
    for (int i = 0; i < end->buttons.mask_len * 8; i++)
        EXPECT_FALSE(XIMaskIsSet(end->buttons.mask, i)) << "mask down for button " << i;

    XFreeEventData(Display(), &ev.xcookie);
}


static const int WINDOW_HIERARCHY_DEPTH = 5;
struct event_history_test {
    int window_depth;         /* window hierarchy depth to be created */
    int grab_window_idx;      /* grab window idx in that hierarchy */
    int event_window_idx;     /* event window idx in that hierarchy */
};

void PrintTo(const struct event_history_test &eht, ::std::ostream *os) {
    *os << "depth: " << eht.window_depth << " grab window idx: " <<
        eht.grab_window_idx << " event window idx: " <<
        eht.event_window_idx;
}

/**
 * @tparam depth of window hierarchy
 */
class TouchEventHistoryTest : public TouchTest,
                              public ::testing::WithParamInterface<struct event_history_test> {};

std::vector<Window> create_window_hierarchy(Display *dpy, int depth) {
    Window root = DefaultRootWindow(dpy);

    std::vector<Window> windows;

    windows.push_back(root);

    Window parent = root;
    Window top_parent = None;
    while(depth--) {
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

TEST_P(TouchEventHistoryTest, EventHistoryReplay) {
    struct event_history_test test_params = GetParam();
    std::stringstream ss;
    ss << "Window hierarchy depth: " << test_params.window_depth << "\n";
    ss << "Grab window is window #" << test_params.grab_window_idx << "\n";
    ss << "Event window is window #" << test_params.event_window_idx << "\n";

    XORG_TESTCASE("Create a window hierarchy\n"
                  "Create a passive touch grab on one of the windows\n"
                  "Select for touch events on the same or a child window\n"
                  "Trigger touch grab\n"
                  "Receive all touch events\n"
                  "Reject touch grab\n"
                  "Receive replayed touch events\n"
                  "Compare original and replayed touch events for equality\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=55477");
    SCOPED_TRACE(ss.str());

    std::vector<Window> windows;
    windows = create_window_hierarchy(Display(), test_params.window_depth);
    Window grab_window = windows[test_params.grab_window_idx];
    Window event_window = windows[test_params.event_window_idx];

    XIEventMask mask;
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);
    XISelectEvents(Display(), event_window, &mask, 1);
    XSync(Display(), False);

    XIGrabModifiers mods = {};
    mods.modifiers = XIAnyModifier;
    ASSERT_EQ(Success, XIGrabTouchBegin(Display(), VIRTUAL_CORE_POINTER_ID,
                                        grab_window, False, &mask, 1, &mods));

    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_TouchBegin));

    std::vector<XIDeviceEvent> events; /* static copy, don't use pointers */
    int touchid = -1;

    while(XPending(Display())) {
        XEvent ev;
        XNextEvent(Display(), &ev);
        ASSERT_EQ(ev.type, GenericEvent);
        ASSERT_EQ(ev.xcookie.extension, xi2_opcode);

        ASSERT_TRUE(XGetEventData(Display(), &ev.xcookie));
        XIDeviceEvent *de = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);

        if (touchid < 0)
            touchid = de->detail;
        else
            ASSERT_EQ(de->detail, touchid);

        ASSERT_EQ(de->deviceid, VIRTUAL_CORE_POINTER_ID);
        ASSERT_EQ(de->event, grab_window);

        events.push_back(*de);

        if (ev.type == XI_TouchEnd)
            break;
    }

    ASSERT_GT(events.size(), 0UL);
    ASSERT_EQ(events.front().evtype, XI_TouchBegin);
    ASSERT_EQ(events.back().evtype, XI_TouchEnd);

    XIAllowTouchEvents(Display(), VIRTUAL_CORE_POINTER_ID, touchid,
                       grab_window, XIRejectTouch);
    XSync(Display(), False);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_TouchBegin));
    for (std::vector<XIDeviceEvent>::const_iterator it = events.begin();
            it != events.end();
            it++)
    {
        XEvent ev;
        XNextEvent(Display(), &ev);

        ASSERT_TRUE(XGetEventData(Display(), &ev.xcookie));

        XIDeviceEvent *current;
        const XIDeviceEvent *old;
        old = &(*it);
        current = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);

        EXPECT_EQ(old->evtype, current->evtype);
        EXPECT_EQ(old->deviceid, current->deviceid);
        EXPECT_EQ(old->detail, current->detail);
        EXPECT_EQ(old->root_x, current->root_x);
        EXPECT_EQ(old->root_y, current->root_y);
        EXPECT_EQ(old->event_x, current->event_x);
        EXPECT_EQ(old->event_y, current->event_y);

        EXPECT_EQ(current->event, event_window);
    }
}

static std::vector<struct event_history_test> generate_event_history_test_cases(int max_depth) {
    std::vector<struct event_history_test> v;

    for (int wd = 0; wd < max_depth; wd++) {
        for (int gwidx = 0; gwidx < wd; gwidx++) {
            for (int ewidx = gwidx; ewidx < wd; ewidx++) {
                struct event_history_test eht = {
                    /* .window_depth =  */ wd,
                    /* .grab_window_idx = */ gwidx,
                    /* .event_window_id = */ ewidx
                };
                v.push_back(eht);
            }
        }
    }
    return v;
}

INSTANTIATE_TEST_CASE_P(, TouchEventHistoryTest,
                        ::testing::ValuesIn(generate_event_history_test_cases(WINDOW_HIERARCHY_DEPTH)));


class TouchDeviceChangeTest : public TouchTest {
protected:
    virtual void SetUp() {
        mouse = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "/mice/PIXART-USB-OPTICAL-MOUSE.desc")
                );

        TouchTest::SetUp();
    }

    virtual void SetUpConfigAndLog() {
        config.AddInputSection("evdev", "mouse",
                               "Option \"CorePointer\" \"on\""
                               "Option \"GrabDevice\" \"on\"\n"
                               "Option \"AccelerationProfile\" \"-1\""
                               "Option \"Device\" \"" + mouse->GetDeviceNode() + "\"");
        TouchTest::SetUpConfigAndLog();
    }

    std::auto_ptr<xorg::testing::evemu::Device> mouse;
};

TEST_F(TouchDeviceChangeTest, NoCursorJumpsOnTouchToPointerSwitch)
{
    XORG_TESTCASE("Create a touch device and a mouse device.\n"
                  "Move pointer to bottom right corner through the mouse\n"
                  "Touch and release from the touch device\n"
                  "Move pointer\n"
                  "Expect pointer motion close to touch end coordinates\n");

    ::Display *dpy = Display();

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

TEST_F(TouchDeviceChangeTest, DeviceChangedEventTouchToPointerSwitch)
{
    XORG_TESTCASE("Create a touch device and a mouse device.\n"
                  "Touch and release from the touch device\n"
                  "Move the mouse device\n"
                  "Expect a DeviceChangedEvent for the mouse on the VCP\n");

    ::Display *dpy = Display();

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    XSync(dpy, False);

    XIEventMask mask;
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask_len = XIMaskLen(XI_DeviceChanged);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_DeviceChanged);

    ASSERT_EQ(Success,
              XISelectEvents(Display(), DefaultRootWindow(Display()), &mask, 1));
    XSync(Display(), False);
    delete[] mask.mask;
    mouse->PlayOne(EV_REL, REL_X, 1, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_DeviceChanged));
    XEvent ev;
    XNextEvent(dpy, &ev);
    ASSERT_EQ(ev.type, GenericEvent);
    ASSERT_EQ(ev.xcookie.extension, xi2_opcode);
    ASSERT_EQ(ev.xcookie.evtype, XI_DeviceChanged);
    ASSERT_TRUE(XGetEventData(Display(), &ev.xcookie));

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, "mouse", &deviceid), 1);

    XIDeviceChangedEvent *cev = reinterpret_cast<XIDeviceChangedEvent*>(ev.xcookie.data);
    ASSERT_EQ(cev->deviceid, VIRTUAL_CORE_POINTER_ID);
    ASSERT_EQ(cev->sourceid, deviceid);
    ASSERT_EQ(cev->reason, XISlaveSwitch);
}

TEST_F(TouchDeviceChangeTest, DeviceChangedEventPointerToTouchSwitch)
{
    XORG_TESTCASE("Create a touch device and a mouse device.\n"
                  "Move the mouse device\n"
                  "Touch and release from the touch device\n"
                  "Expect a DeviceChangedEvent for the touch device on the VCP\n");

    ::Display *dpy = Display();

    mouse->PlayOne(EV_REL, REL_X, 1, true);

    XIEventMask mask;
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask_len = XIMaskLen(XI_DeviceChanged);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_DeviceChanged);

    ASSERT_EQ(Success,
              XISelectEvents(Display(), DefaultRootWindow(Display()), &mask, 1));
    XSync(Display(), False);
    delete[] mask.mask;

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    XSync(dpy, False);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_DeviceChanged));
    XEvent ev;
    XNextEvent(dpy, &ev);
    ASSERT_EQ(ev.type, GenericEvent);
    ASSERT_EQ(ev.xcookie.extension, xi2_opcode);
    ASSERT_EQ(ev.xcookie.evtype, XI_DeviceChanged);
    ASSERT_TRUE(XGetEventData(Display(), &ev.xcookie));

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, "N-Trig MultiTouch", &deviceid), 1);

    XIDeviceChangedEvent *cev = reinterpret_cast<XIDeviceChangedEvent*>(ev.xcookie.data);
    ASSERT_EQ(cev->deviceid, VIRTUAL_CORE_POINTER_ID);
    ASSERT_EQ(cev->sourceid, deviceid);
    ASSERT_EQ(cev->reason, XISlaveSwitch);

}

TEST_F(TouchTest, JumpingCursorOnTransformationMatrix)
{
    XORG_TESTCASE("Create a touch device\n"
                  "Set transformation matrix\n"
                  "Move pointer to location x/y\n"
                  "Move pointer by x, leaving y untouched\n"
                  "Verify that pointer has moved, but y has stayed the same\n"
                  "Repeat above with x axis scaled and y unscaled\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=49347");

    ::Display *dpy = Display();

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, "N-Trig MultiTouch", &deviceid), 1);

    Atom matrix_prop = XInternAtom(dpy, "Coordinate Transformation Matrix", True);
    ASSERT_NE(matrix_prop, (Atom)None);
    Atom float_prop = XInternAtom(dpy, "FLOAT", True);
    ASSERT_NE(float_prop, (Atom)None);

    float matrix[9] = {0};

    /* Test with Y coordinate scaled */

    matrix[0] = 1;
    matrix[4] = 0.5; /* scale y to half */
    matrix[8] = 1;

    XIChangeProperty(dpy, deviceid, matrix_prop, float_prop, 32,
                     XIPropModeReplace,
                     reinterpret_cast<unsigned char*>(matrix), 9);
    XSync(dpy, False);

    TouchBegin(2745, 1639);

    double x1, y1;
    double x2, y2;

    QueryPointerPosition(dpy, &x1, &y1);
    TouchUpdate(5000,1639);
    QueryPointerPosition(dpy, &x2, &y2);

    ASSERT_NE(x1, x2);
    ASSERT_EQ(y1, y2);

    TouchEnd();

    /* Test with X coordinate scaled */

    matrix[0] = 0.5; /* scale x to half */
    matrix[4] = 1;
    matrix[8] = 1;

    XIChangeProperty(dpy, deviceid, matrix_prop, float_prop, 32,
                     XIPropModeReplace,
                     reinterpret_cast<unsigned char*>(matrix), 9);
    XSync(dpy, False);

    TouchBegin(2745, 1639);
    QueryPointerPosition(dpy, &x1, &y1);
    TouchUpdate(2745, 2000);
    QueryPointerPosition(dpy, &x2, &y2);

    ASSERT_EQ(x1, x2);
    ASSERT_NE(y1, y2);

    TouchEnd();
}

#endif /* HAVE_XI22 */

INSTANTIATE_TEST_CASE_P(, TouchTestXI2Version, ::testing::Range(0, XI_2_Minor + 1));

