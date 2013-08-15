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

#include <tr1/tuple>

#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XInput.h>

#include "xit-event.h"
#include "xit-server-input-test.h"
#include "device-interface.h"
#include "helpers.h"


/**
 * Evdev driver test for keyboard devices. Takes a string as parameter,
 * which is later used for the XkbLayout option.
 */
class PointerGrabTest : public XITServerInputTest,
                        public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");

        xi2_major_minimum = 2;
        xi2_minor_minimum = 1;

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

TEST_F(PointerGrabTest, ImplicitGrabRawEvents)
{
    XORG_TESTCASE("This test registers for XI_ButtonPress on a \n"
                  "window and expects raw events to arrive while the \n"
                  "implicit grab is active.\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=53897");

    ::Display *dpy = Display();

    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
                                     DisplayWidth(dpy, DefaultScreen(dpy)),
                                     DisplayHeight(dpy, DefaultScreen(dpy)),
                                     0, 0,
                                     WhitePixel(dpy, DefaultScreen(dpy)));
    XSelectInput(dpy, win, ExposureMask);
    XMapWindow(dpy, win);
    XFlush(dpy);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           Expose,
                                                           -1, -1, 1000));

    XIEventMask mask;

    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_RawMotion);
    mask.mask = new unsigned char[mask.mask_len];
    memset(mask.mask, 0, mask.mask_len);
    XISetMask(mask.mask, XI_ButtonPress);
    XISelectEvents(dpy, win, &mask, 1);

    memset(mask.mask, 0, mask.mask_len);
    XISetMask(mask.mask, XI_RawButtonPress);
    XISetMask(mask.mask, XI_RawMotion);
    XISelectEvents(dpy, DefaultRootWindow(dpy), &mask, 1);

    delete mask.mask;

    XSync(dpy, False);

    /* Move pointer, make sure we receive raw events */
    dev->PlayOne(EV_REL, REL_X, -1, true);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_RawMotion,
                                                           1000));
    XEvent ev;
    XNextEvent(dpy, &ev);
    ASSERT_TRUE(NoEventPending(dpy));

    /* Now press button, make sure sure we still get raw events */
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);

    /* raw button events come before normal button events */
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_RawButtonPress,
                                                           1000));
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonPress,
                                                           1000));
    dev->PlayOne(EV_REL, REL_X, -1, true);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_RawMotion,
                                                           1000));
    XNextEvent(dpy, &ev);
    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_F(PointerGrabTest, GrabDisabledDevices)
{
    XORG_TESTCASE("Grabbing a disabled device must not segfault the server.\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=54934");

    Atom prop = XInternAtom(Display(), "Device Enabled", True);
    ASSERT_NE(prop, (Atom)None);

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    unsigned char data = 0;
    XIChangeProperty(Display(), deviceid, prop, XA_INTEGER, 8,
            PropModeReplace, &data, 1);
    XFlush(Display());

    XDevice *xdev = XOpenDevice(Display(), deviceid);
    ASSERT_TRUE(xdev != NULL);

    XGrabDevice(Display(), xdev, DefaultRootWindow(Display()), True,
            0, NULL, GrabModeAsync, GrabModeAsync, CurrentTime);
    XSync(Display(), False);
    ASSERT_EQ(server.GetState(), xorg::testing::Process::RUNNING);
}

TEST_F(PointerGrabTest, DestroyWindowDuringImplicitGrab)
{
    XORG_TESTCASE("Create a window with the button masks set.\n"
                  "Create a pointer device\n"
                  "Activate implicit pointer grab by clicking into the window.\n"
                  "While the implicit grab is active, destroy the window\n");

    ::Display *dpy = Display();
    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
                                     DisplayWidth(dpy, DefaultScreen(dpy)),
                                     DisplayHeight(dpy, DefaultScreen(dpy)),
                                     0, 0, 0);
    ASSERT_NE(win, (Window)None);
    XSelectInput(dpy, win, StructureNotifyMask|ButtonPressMask|ButtonReleaseMask);
    XMapWindow(dpy, win);
    XSync(dpy, False);

    if (xorg::testing::XServer::WaitForEventOfType(dpy, MapNotify, -1, -1)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
    } else {
        ADD_FAILURE() << "Failed waiting for Exposure";
    }

    XSync(dpy, True);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, ButtonPress, -1, -1));

    XDestroyWindow(dpy, win);
    XSync(dpy, False);

    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    XSync(dpy, False);
    ASSERT_EQ(server.GetState(), xorg::testing::Process::RUNNING);
}

TEST_F(PointerGrabTest, DestroyClientDuringImplicitGrab)
{
    XORG_TESTCASE("Create a window with the button masks set.\n"
                  "Create a pointer device\n"
                  "Activate implicit pointer grab by clicking into the window.\n"
                  "While the implicit grab is active, destroy the client\n");

    ::Display *dpy = Display();
    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
                                     DisplayWidth(dpy, DefaultScreen(dpy)),
                                     DisplayHeight(dpy, DefaultScreen(dpy)),
                                     0, 0, 0);
    ASSERT_NE(win, (Window)None);
    XSelectInput(dpy, win, StructureNotifyMask|ButtonPressMask|ButtonReleaseMask);
    XMapWindow(dpy, win);
    XSync(dpy, False);

    if (xorg::testing::XServer::WaitForEventOfType(dpy, MapNotify, -1, -1)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
    } else {
        ADD_FAILURE() << "Failed waiting for Exposure";
    }

    XSync(dpy, True);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, ButtonPress, -1, -1));

    XCloseDisplay(dpy);

    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    ASSERT_EQ(server.GetState(), xorg::testing::Process::RUNNING);
}

TEST_F(PointerGrabTest, ImplicitGrabToActiveGrab)
{
    XORG_TESTCASE("Create a window with the button masks set.\n"
                  "Create a pointer device\n"
                  "Activate implicit pointer grab by clicking into the window.\n"
                  "While the implicit grab is active, grab the pointer\n"
                  "Release the button, check event is received\n");

    ::Display *dpy = Display();
    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
                                     DisplayWidth(dpy, DefaultScreen(dpy)),
                                     DisplayHeight(dpy, DefaultScreen(dpy)),
                                     0, 0, 0);
    ASSERT_NE(win, (Window)None);
    XSelectInput(dpy, win, StructureNotifyMask|ButtonPressMask|ButtonReleaseMask);
    XMapWindow(dpy, win);
    XSync(dpy, False);

    if (xorg::testing::XServer::WaitForEventOfType(dpy, MapNotify, -1, -1)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
    } else {
        ADD_FAILURE() << "Failed waiting for Exposure";
    }

    XSync(dpy, True);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, ButtonPress, -1, -1));

    ASSERT_EQ(Success, XGrabPointer(dpy, win, True, ButtonReleaseMask,
                GrabModeAsync, GrabModeAsync, None, None, CurrentTime));
    XSync(dpy, False);

    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, ButtonRelease, -1, -1));
    ASSERT_EQ(server.GetState(), xorg::testing::Process::RUNNING);
}

TEST_F(PointerGrabTest, ImplicitGrabToActiveGrabDeactivated)
{
    XORG_TESTCASE("Create a window with the button masks set.\n"
                  "Create a pointer device\n"
                  "Activate implicit pointer grab by clicking into the window.\n"
                  "While the implicit grab is active, grab the pointer\n"
                  "Ungrab the pointer.\n"
                  "Release the button\n");

    ::Display *dpy = Display();
    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
                                     DisplayWidth(dpy, DefaultScreen(dpy)),
                                     DisplayHeight(dpy, DefaultScreen(dpy)),
                                     0, 0, 0);
    ASSERT_NE(win, (Window)None);
    XSelectInput(dpy, win, StructureNotifyMask|ButtonPressMask|ButtonReleaseMask);
    XMapWindow(dpy, win);
    XSync(dpy, False);

    if (xorg::testing::XServer::WaitForEventOfType(dpy, MapNotify, -1, -1)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
    } else {
        ADD_FAILURE() << "Failed waiting for Exposure";
    }

    XSync(dpy, True);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, ButtonPress, -1, -1));

    ASSERT_EQ(Success, XGrabPointer(dpy, win, True, 0,
                                    GrabModeAsync, GrabModeAsync, None,
                                    None, CurrentTime));
    XSync(dpy, False);
    XUngrabPointer(dpy, CurrentTime);
    XSync(dpy, False);

    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, ButtonRelease, -1, -1));
    ASSERT_EQ(server.GetState(), xorg::testing::Process::RUNNING);
}

TEST_F(PointerGrabTest, AsyncPassiveGrabPressRelease)
{
    XORG_TESTCASE("Client 2 creates window with button mask.\n"
                  "Client 1 async passive button grab on that window.\n"
                  "Double-click.\n"
                  "Client 1 should get press/release\n"
                  "Client 2 gets nothing\n"
                  "Ungrab button, replay, now C2 gets the events\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    XSynchronize(dpy1, True);
    XSynchronize(dpy2, True);

    Window win = CreateWindow(dpy2, None);
    XSelectInput(dpy2, win, ButtonPressMask|ButtonReleaseMask);

    XGrabButton(dpy1, AnyButton, XIAnyModifier, win, False,
                ButtonPressMask|ButtonReleaseMask,
                GrabModeAsync, GrabModeAsync, None, None);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    for (int i = 0; i < 2; i++) {
        ASSERT_EVENT(XEvent, press, dpy1, ButtonPress);
        ASSERT_EVENT(XEvent, release, dpy1, ButtonRelease);
    }

    XUngrabButton(dpy1, AnyButton, XIAnyModifier, win);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    for (int i = 0; i < 2; i++) {
        ASSERT_EVENT(XEvent, press, dpy2, ButtonPress);
        ASSERT_EVENT(XEvent, release, dpy2, ButtonRelease);
    }

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

/**
 * @tparam AsyncPointer, SyncPointer, ReplayPointer
 */
class PointerGrabTestAllowEvents : public PointerGrabTest,
                                        public ::testing::WithParamInterface<int> {
};

TEST_P(PointerGrabTestAllowEvents, AllowEventsDoubleClick)
{
    int mode = GetParam();
    std::string strmode;
    switch(mode) {
        case ReplayPointer:     strmode = "ReplayPointer"; break;
        case AsyncPointer:      strmode = "AsyncPointer"; break;
        case SyncPointer:       strmode = "SyncPointer"; break;
    }
    XORG_TESTCASE("Client 2 creates window with button mask.\n"
                  "Client 1 sync passive button grab on that window.\n"
                  "Double-click.\n"
                  "Client1: AllowEvents(" + strmode + ")\n"
                  "Client2: events for Replay, not for Sync/Async\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    XSynchronize(dpy1, True);
    XSynchronize(dpy2, True);

    Window win = CreateWindow(dpy2, None);
    XSelectInput(dpy2, win, ButtonPressMask|ButtonReleaseMask);

    XGrabButton(dpy1, AnyButton, XIAnyModifier, win, False,
                ButtonPressMask|ButtonReleaseMask,
                GrabModeSync, GrabModeAsync, None, None);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    for (int i = 0; i < 2; i++) {
        ASSERT_EVENT(XEvent, press, dpy1, ButtonPress);
        XAllowEvents(dpy1, mode, CurrentTime);
        if (mode == ReplayPointer) {
            ASSERT_EVENT(XEvent, press, dpy2, ButtonPress);
            ASSERT_EVENT(XEvent, release, dpy2, ButtonRelease);
        } else {
            ASSERT_EVENT(XEvent, release, dpy1, ButtonRelease);
        }
    }

    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
}

INSTANTIATE_TEST_CASE_P(, PointerGrabTestAllowEvents,
                        ::testing::Values(AsyncPointer, SyncPointer, ReplayPointer));

enum GrabType {
    GRABTYPE_CORE,
    GRABTYPE_XI1,
    GRABTYPE_XI2,
    GRABTYPE_XI2TOUCH,
};

static std::string grabtype_enum_to_string(enum GrabType gt) {
    switch(gt) {
        case GRABTYPE_CORE: return "GRABTYPE_CORE"; break;
        case GRABTYPE_XI1:  return "GRABTYPE_XI1"; break;
        case GRABTYPE_XI2:  return "GRABTYPE_XI2"; break;
        case GRABTYPE_XI2TOUCH:  return "GRABTYPE_XI2TOUCH"; break;
    }

    ADD_FAILURE() << "BUG: shouldn't get here";
    return NULL;
}

class PointerSingleGrabTypeTest : public PointerGrabTest,
                                  public ::testing::WithParamInterface<enum ::GrabType> {};

TEST_P(PointerSingleGrabTypeTest, OverwriteGrab)
{
    XORG_TESTCASE("Grab the pointer.\n"
                  "Re-grab with a different event mask\n");

    enum GrabType grab_type = GetParam();
    std::stringstream ss;
    ss << grabtype_enum_to_string(grab_type);
    SCOPED_TRACE(ss.str());

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    int rc;

    XIEventMask mask;
    unsigned char m[XIMaskLen(XI_LASTEVENT)] = {0};
    mask.mask_len = sizeof(m);
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask = m;
    XISetMask(mask.mask, XI_Motion);


    int deviceid;
    FindInputDeviceByName(dpy, "--device--", &deviceid);
    XDevice *xdev = XOpenDevice(dpy, deviceid);
    XEventClass cls;
    int xi_motion;
    DeviceMotionNotify(xdev, xi_motion, cls);

    switch(grab_type) {
        case GRABTYPE_CORE:
            rc = XGrabPointer(dpy, root, False, PointerMotionMask,
                              GrabModeAsync, GrabModeAsync, None, None,
                              CurrentTime);
            ASSERT_EQ(rc, Success);
            break;
        case GRABTYPE_XI1:
            rc = XGrabDevice(dpy, xdev, root, False, 1, &cls, GrabModeAsync,
                             GrabModeAsync, CurrentTime);
            break;
        case GRABTYPE_XI2:
            rc = XIGrabDevice(Display(), VIRTUAL_CORE_POINTER_ID, root, CurrentTime, None,
                              GrabModeAsync, GrabModeAsync, False, &mask);
            break;
    }
    ASSERT_EQ(rc, Success);

    dev->PlayOne(EV_REL, REL_X, -1, true);

    switch(grab_type) {
        case GRABTYPE_CORE:
            {
                ASSERT_EVENT(XEvent, motion, dpy, MotionNotify);
                ASSERT_TRUE(NoEventPending(dpy));
                rc = XGrabPointer(dpy, root, False, 0,
                                  GrabModeAsync, GrabModeAsync, None, None,
                                  CurrentTime);
                ASSERT_EQ(rc, Success);
                break;
            }
        case GRABTYPE_XI1:
            {
                ASSERT_EVENT(XEvent, xi_motion, dpy, xi_motion);
                ASSERT_TRUE(NoEventPending(dpy));
                rc = XGrabDevice(dpy, xdev, root, False, 0, NULL, GrabModeAsync,
                                 GrabModeAsync, CurrentTime);
                break;
            }
        case GRABTYPE_XI2:
            {
                ASSERT_EVENT(XIDeviceEvent, motion, dpy,
                             GenericEvent, xi2_opcode, XI_Motion);
                ASSERT_TRUE(NoEventPending(dpy));

                XIClearMask(mask.mask, XI_Motion);
                rc = XIGrabDevice(Display(), VIRTUAL_CORE_POINTER_ID, root, CurrentTime, None,
                                  GrabModeAsync, GrabModeAsync, False, &mask);
                ASSERT_EQ(rc, Success);
                break;
            }
    }

    dev->PlayOne(EV_REL, REL_X, -1, true);

    ASSERT_TRUE(NoEventPending(dpy));
}

INSTANTIATE_TEST_CASE_P(, PointerSingleGrabTypeTest, ::testing::Values(GRABTYPE_CORE, GRABTYPE_XI1, GRABTYPE_XI2));

class PointerGrabTypeTest : public PointerGrabTest,
                            public
                            ::testing::WithParamInterface<std::tr1::tuple<enum ::GrabType, enum GrabType> > {};

TEST_P(PointerGrabTypeTest, DifferentGrabTypesProhibited)
{
    XORG_TESTCASE("Grab the pointer.\n"
                  "Re-grab the pointer with a different type.\n"
                  "Expect AlreadyGrabbed for different types\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=58255");

    std::tr1::tuple<enum GrabType, enum GrabType> t = GetParam();
    enum GrabType grab_type_1 = std::tr1::get<0>(t);
    enum GrabType grab_type_2 = std::tr1::get<1>(t);
    std::stringstream ss;
    ss << grabtype_enum_to_string(grab_type_1) << " vs. " << grabtype_enum_to_string(grab_type_2);
    SCOPED_TRACE(ss.str());

    ::Display *dpy = Display();
    XSynchronize(dpy, True);
    Window root = DefaultRootWindow(dpy);
    Status rc, expected_status = AlreadyGrabbed;
    int deviceid;
    XDevice *device = NULL;
    XIEventMask mask;
    mask.mask_len = 0;

#define VIRTUAL_CORE_POINTER_XTEST_ID 4

    if (grab_type_1 == grab_type_2) /* Overwriting grabs is permitted */
        expected_status = GrabSuccess;

    if (grab_type_1 == GRABTYPE_XI1 || grab_type_2 == GRABTYPE_XI1) /* XI1 can't grab the VCP */
        deviceid = VIRTUAL_CORE_POINTER_XTEST_ID;
    else
        deviceid = VIRTUAL_CORE_POINTER_ID;

    switch(grab_type_1) {
        case GRABTYPE_CORE:
            rc = XGrabPointer(dpy, root, true, 0, GrabModeAsync, GrabModeAsync, None,
                              None, CurrentTime);
            break;
        case GRABTYPE_XI2:
            rc = XIGrabDevice(dpy, deviceid, root, CurrentTime, None,
                              GrabModeAsync, GrabModeAsync, True, &mask);
            break;
        case GRABTYPE_XI1:
            deviceid = VIRTUAL_CORE_POINTER_XTEST_ID;
            device = XOpenDevice(dpy, deviceid);
            ASSERT_TRUE(device);
            rc = XGrabDevice(dpy, device, root, False, 0, NULL,
                             GrabModeAsync, GrabModeAsync, CurrentTime);
            break;
        default:
            FAIL();
            break;

    }
    ASSERT_EQ(rc, GrabSuccess);

    switch(grab_type_2) {
        case GRABTYPE_CORE:
            if (grab_type_1 == GRABTYPE_XI1)
                expected_status = GrabSuccess;
            rc = XGrabPointer(dpy, root, true, 0, GrabModeAsync, GrabModeAsync, None,
                              None, CurrentTime);
            break;
        case GRABTYPE_XI2:
            rc = XIGrabDevice(dpy, deviceid, root, CurrentTime, None,
                              GrabModeAsync, GrabModeAsync, True, &mask);

            break;
        case GRABTYPE_XI1:
            if (grab_type_1 == GRABTYPE_CORE)
                expected_status = GrabSuccess;
            if (!device)
                device = XOpenDevice(dpy, deviceid);
            ASSERT_TRUE(device);
            rc = XGrabDevice(dpy, device, root, False, 0, NULL,
                             GrabModeAsync, GrabModeAsync, CurrentTime);
            break;
        default:
            FAIL();
            break;
    }

    ASSERT_EQ(rc, expected_status);

    if (device)
        XCloseDevice(dpy, device);
}

INSTANTIATE_TEST_CASE_P(, PointerGrabTypeTest,
        ::testing::Combine(
            ::testing::Values(GRABTYPE_CORE, GRABTYPE_XI1, GRABTYPE_XI2),
            ::testing::Values(GRABTYPE_CORE, GRABTYPE_XI1, GRABTYPE_XI2)));

#if HAVE_XI22
class TouchGrabTest : public XITServerInputTest,
                      public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device.
     */
    virtual void SetUp() {
        SetDevice("tablets/N-Trig-MultiTouch.desc");

        xi2_major_minimum = 2;
        xi2_minor_minimum = 2;

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

    /**
     * Return a new synchronized client given our default server connection.
     */
    virtual ::Display* NewClient(int maj = 2, int min = 2) {
        ::Display *d = XOpenDisplay(server.GetDisplayString().c_str());
        if (!d)
            ADD_FAILURE() << "Failed to open display for new client.\n";
        XSynchronize(d, True);
        int major = maj, minor = min;
        if (maj >= 2 && XIQueryVersion(d, &major, &minor) != Success)
            ADD_FAILURE() << "XIQueryVersion failed on new client.\n";
        return d;
    }

    virtual void GrabDevice(::Display *dpy, int deviceid, Window win, bool ownership = false)
    {
        XIEventMask mask;
        mask.deviceid = deviceid;
        mask.mask_len = XIMaskLen(XI_TouchOwnership);
        mask.mask = new unsigned char[mask.mask_len]();

        XISetMask(mask.mask, XI_TouchBegin);
        XISetMask(mask.mask, XI_TouchEnd);
        XISetMask(mask.mask, XI_TouchUpdate);

        if (ownership)
            XISetMask(mask.mask, XI_TouchOwnership);

        ASSERT_EQ(Success, XIGrabDevice(dpy, deviceid,
                                        win, CurrentTime, None,
                                        GrabModeAsync, GrabModeAsync,
                                        False, &mask));
        delete[] mask.mask;
        XSync(dpy, False);
    }

    virtual void GrabPointer(::Display *dpy, int deviceid, Window win)
    {
        XIEventMask mask;
        mask.deviceid = deviceid;
        mask.mask_len = XIMaskLen(XI_TouchOwnership);
        mask.mask = new unsigned char[mask.mask_len]();

        XISetMask(mask.mask, XI_ButtonPress);
        XISetMask(mask.mask, XI_ButtonRelease);
        XISetMask(mask.mask, XI_Motion);

        ASSERT_EQ(Success, XIGrabDevice(dpy, deviceid,
                                        win, CurrentTime, None,
                                        GrabModeAsync, GrabModeAsync,
                                        False, &mask));
        delete[] mask.mask;
        XSync(dpy, False);
    }

    virtual void TouchBegin(int x, int y) {
        dev->PlayOne(EV_KEY, BTN_TOUCH, 1);
        TouchUpdate(x, y);
    }

    virtual void TouchUpdate(int x, int y, bool sync = true) {
        dev->PlayOne(EV_ABS, ABS_X, x);
        dev->PlayOne(EV_ABS, ABS_Y, y);
        dev->PlayOne(EV_ABS, ABS_MT_POSITION_X, x);
        dev->PlayOne(EV_ABS, ABS_MT_POSITION_Y, y);
        /* same values as the recordings file */
        dev->PlayOne(EV_ABS, ABS_MT_ORIENTATION, 0);
        dev->PlayOne(EV_ABS, ABS_MT_TOUCH_MAJOR, 468);
        dev->PlayOne(EV_ABS, ABS_MT_TOUCH_MINOR, 306);
        dev->PlayOne(EV_SYN, SYN_MT_REPORT, 0);
        if (sync)
            dev->PlayOne(EV_SYN, SYN_REPORT, 0);
    }

    virtual void TouchEnd() {
        dev->PlayOne(EV_KEY, BTN_TOUCH, 0);
        dev->PlayOne(EV_SYN, SYN_REPORT, 0);
    }

};

TEST_F(TouchGrabTest, ActivePointerGrabOverPointerSelection)
{
    XORG_TESTCASE("Client c1 creates window and selects for button press.\n"
                  "Client c2 has async active grab on the window\n"
                  "Touch the window\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = NewClient();

    Window win = CreateWindow(dpy1, DefaultRootWindow(dpy1));

    SelectXI2Events(dpy1, XIAllMasterDevices, win,
                    XI_ButtonPress,
                    XI_ButtonRelease,
                    XI_Motion,
                    -1);
    GrabPointer(dpy2, VIRTUAL_CORE_POINTER_ID, win);

    TouchBegin(200, 200);
    TouchUpdate(282, 282);
    TouchEnd();

    ASSERT_EVENT(XIDeviceEvent, press, dpy2, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_EVENT(XIDeviceEvent, motion, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(XIDeviceEvent, release, dpy2, GenericEvent, xi2_opcode, XI_ButtonRelease);

    ASSERT_TRUE(NoEventPending(dpy2));

    TouchBegin(210, 210);
    TouchUpdate(292, 292);
    TouchEnd();

    ASSERT_EVENT(XIDeviceEvent, press2, dpy2, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_EVENT(XIDeviceEvent, motion2, dpy2, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(XIDeviceEvent, release2, dpy2, GenericEvent, xi2_opcode, XI_ButtonRelease);

    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy1));

    XCloseDisplay(dpy2);
}

TEST_F(TouchGrabTest, PassiveTouchGrabPassedToTouchClient)
{
    XORG_TESTCASE("Client 1: register for passive touch grab on roow window\n"
                  "Client 2: register for touch events on window\n"
                  "Trigger touch begin/end\n"
                  "Client 1: reject after TouchEnd\n"
                  "Client 2: verify touch event is received\n"
                  "Verify pointer has no button set\n");

    int major = 2, minor = 2;
    ::Display *dpy1 = Display();
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    XSynchronize(dpy2, True);
    XIQueryVersion(dpy2, &major, &minor);

    Window root = DefaultRootWindow(dpy1);
    Window win = CreateWindow(dpy2, None);

    XIEventMask mask;
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    XIGrabModifiers mods = {};
    mods.modifiers = XIAnyModifier;
    ASSERT_EQ(Success, XIGrabTouchBegin(dpy1, VIRTUAL_CORE_POINTER_ID,
                                        root, False, &mask, 1, &mods));
    XISelectEvents(dpy2, win, &mask, 1);
    delete[] mask.mask;

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    ASSERT_EVENT(XIDeviceEvent, tbegin, dpy1, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_EVENT(XIDeviceEvent, tend, dpy1, GenericEvent, xi2_opcode, XI_TouchEnd);
    XIAllowTouchEvents(dpy1, tbegin->deviceid, tbegin->detail, root, XIRejectTouch);

    ASSERT_EVENT(XEvent, tbegin2, dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_EVENT(XEvent, tend2, dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy1));

    /* use a third client for checking pointer state, since a XI2.2 client
     * won't see emulated touch state */
    ::Display *dpy3 = XOpenDisplay(server.GetDisplayString().c_str());
    XSynchronize(dpy3, True);
    Window child;
    double win_x, win_y;
    double root_x, root_y;
    XIButtonState buttons;
    XIModifierState modifiers;
    XIGroupState group;
    XIQueryPointer(dpy3, VIRTUAL_CORE_POINTER_ID,
                   root, &root, &child,
                   &root_x, &root_y,
                   &win_x, &win_y,
                   &buttons, &modifiers, &group);

    ASSERT_GE(buttons.mask_len, 1);
    for (int i = 0; i < buttons.mask_len * 8; i++)
        ASSERT_FALSE(XIMaskIsSet(buttons.mask, i));
}

TEST_F(TouchGrabTest, TouchGrabPassedToCoreGrab)
{
    XORG_TESTCASE("Client 1: Register for passive touch grab on root window\n"
                  "Client 2: Register for passive button grab on client window\n"
                  "Trigger touch begin/end\n"
                  "Client 1: reject after TouchEnd\n"
                  "Client 2: verify button event is received\n"
                  "Repeat 10 times\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    XSynchronize(dpy2, True);

    Window root = DefaultRootWindow(dpy1);
    Window win = CreateWindow(dpy2, None);

    XIEventMask mask;
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    XIGrabModifiers mods = {};
    mods.modifiers = XIAnyModifier;
    ASSERT_EQ(Success, XIGrabTouchBegin(dpy1, VIRTUAL_CORE_POINTER_ID,
                                        root, False, &mask, 1, &mods));
    delete[] mask.mask;

    XGrabButton(dpy2, AnyButton, XIAnyModifier, win, False,
                ButtonPressMask|ButtonReleaseMask,
                GrabModeAsync, GrabModeAsync, None, None);

    for (int i = 0; i < 10; i++) {
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

        ASSERT_EVENT(XIDeviceEvent, tbegin, dpy1, GenericEvent, xi2_opcode, XI_TouchBegin);
        ASSERT_EVENT(XIDeviceEvent, tend, dpy1, GenericEvent, xi2_opcode, XI_TouchEnd);
        XIAllowTouchEvents(dpy1, tbegin->deviceid, tbegin->detail, root, XIRejectTouch);

        ASSERT_EVENT(XEvent, press, dpy2, ButtonPress);
        ASSERT_EVENT(XEvent, release, dpy2, ButtonRelease);
        ASSERT_TRUE(NoEventPending(dpy2));
        ASSERT_TRUE(NoEventPending(dpy1));
    }
}

TEST_F(TouchGrabTest, TouchGrabPassedToTouch)
{
    XORG_TESTCASE("Client C1 has touch grab on root window.\n"
                  "Client c2 has touch grab on window.\n"
                  "Touch begin/end\n"
                  "Reject from C1\n"
                  "Verify C2 gets all events\n");

    int major = 2, minor = 2;
    ::Display *dpy1 = Display();
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    XIQueryVersion(dpy2, &major, &minor);
    XSynchronize(dpy2, True);

    Window root = DefaultRootWindow(dpy1);
    Window win = CreateWindow(dpy2, None);

    XIEventMask mask;
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    XIGrabModifiers mods = {};
    mods.modifiers = XIAnyModifier;
    ASSERT_EQ(Success, XIGrabTouchBegin(dpy1, VIRTUAL_CORE_POINTER_ID,
                                        root, False, &mask, 1, &mods));
    ASSERT_EQ(Success, XIGrabTouchBegin(dpy2, VIRTUAL_CORE_POINTER_ID,
                                        win, False, &mask, 1, &mods));
    delete[] mask.mask;

    for (int i = 0; i < 10; i++) {
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

        ASSERT_EVENT(XIDeviceEvent, tbegin, dpy1, GenericEvent, xi2_opcode, XI_TouchBegin);
        ASSERT_EVENT(XIDeviceEvent, tend, dpy1, GenericEvent, xi2_opcode, XI_TouchEnd);
        XIAllowTouchEvents(dpy1, tbegin->deviceid, tbegin->detail, root, XIRejectTouch);
        ASSERT_TRUE(NoEventPending(dpy1));

        ASSERT_EVENT(XIDeviceEvent, tbegin2, dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
        ASSERT_EVENT(XIDeviceEvent, tend2, dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);
        XIAllowTouchEvents(dpy2, tend2->deviceid, tend2->detail, win, XIAcceptTouch);
        ASSERT_TRUE(NoEventPending(dpy2));
    }
}

TEST_F(TouchGrabTest, TouchGrabPassedToTouchEarlyAccept)
{
    XORG_TESTCASE("Client C1 has touch grab on root window.\n"
                  "Client C2 has touch grab with ownership on window.\n"
                  "Client C3 has touch grab with ownership on window.\n"
                  "Touch begin\n"
                  "Accept touch from C2.\n"
                  "Expect TouchEnd for C3.\n"
                  "Reject from C1\n"
                  "Expect TouchEnd for C1\n"
                  "Expect TouchOwnerhip for C2\n"
                  "Touch end\n"
                  "Expect TouchEnd for C2\n");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    ::Display *dpy3 = XOpenDisplay(server.GetDisplayString().c_str());
    XSynchronize(dpy1, True);
    XSynchronize(dpy2, True);
    XSynchronize(dpy3, True);

    int major = 2, minor = 3;
    XIQueryVersion(dpy2, &major, &minor);
    XIQueryVersion(dpy3, &major, &minor);


    Window root = DefaultRootWindow(dpy1);
    Window win = CreateWindow(dpy3, None);
    Window subwin = CreateWindow(dpy3, win);

    XIEventMask mask;
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    XIGrabModifiers mods = {};
    mods.modifiers = XIAnyModifier;
    ASSERT_EQ(Success, XIGrabTouchBegin(dpy1, VIRTUAL_CORE_POINTER_ID,
                                        root, False, &mask, 1, &mods));

    XISetMask(mask.mask, XI_TouchOwnership);

    ASSERT_EQ(Success, XIGrabTouchBegin(dpy2, VIRTUAL_CORE_POINTER_ID,
                                        win, False, &mask, 1, &mods));
    ASSERT_EQ(Success, XIGrabTouchBegin(dpy3, VIRTUAL_CORE_POINTER_ID,
                                        subwin, False, &mask, 1, &mods));
    delete[] mask.mask;

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    /* C1 is owner, but all three get begin event */
    ASSERT_EVENT(XIDeviceEvent, tbegin1, dpy1, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_EVENT(XIDeviceEvent, tbegin2, dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_EVENT(XIDeviceEvent, tbegin3, dpy3, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_TRUE(NoEventPending(dpy1));
    ASSERT_TRUE(NoEventPending(dpy2));
    ASSERT_TRUE(NoEventPending(dpy3));

    /* Accept from C2, changes nothing on the wire yet */
    XIAllowTouchEvents(dpy2, tbegin2->deviceid, tbegin2->detail, win, XIAcceptTouch);

    /* Reject from C1, finishes for C1, owner is C2 */
    XIAllowTouchEvents(dpy1, tbegin1->deviceid, tbegin1->detail, root, XIRejectTouch);
    ASSERT_EVENT(XIDeviceEvent, tend1, dpy1, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_EVENT(XIDeviceEvent, towner2, dpy2, GenericEvent, xi2_opcode, XI_TouchOwnership);

    /* Physicall end touch */
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");
    ASSERT_EVENT(XIDeviceEvent, tend2, dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(NoEventPending(dpy2));

    ASSERT_EVENT(XIDeviceEvent, tend3, dpy3, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(NoEventPending(dpy3));
}

TEST_F(TouchGrabTest, GrabMultipleTouchpoints)
{
    XORG_TESTCASE("Grab the device with touch mask\n"
                  "Touch repeatedly\n"
                  "Ensure all touches send touch events\n");

    ::Display *dpy = Display();


    int deviceid;
    FindInputDeviceByName(dpy, "--device--", &deviceid);

    Window win = CreateWindow(dpy, DefaultRootWindow(dpy));

    GrabDevice(dpy, deviceid, win);

    /* ntrig has no slots, so the second update actually creates a new point */
    TouchBegin(200, 200);
    TouchUpdate(250, 250, false);
    TouchUpdate(2500, 2500, true);
    TouchUpdate(2700, 2700, true);
    TouchEnd();

    EXPECT_EVENT(XIDeviceEvent, tbegin1, dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
    EXPECT_EVENT(XIDeviceEvent, tupdate1, dpy, GenericEvent, xi2_opcode, XI_TouchUpdate);
    EXPECT_EVENT(XIDeviceEvent, tbegin2, dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
    EXPECT_EVENT(XIDeviceEvent, tupdate2, dpy, GenericEvent, xi2_opcode, XI_TouchUpdate);
    EXPECT_EVENT(XIDeviceEvent, tend1, dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
    EXPECT_EVENT(XIDeviceEvent, tend2, dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
}


class TouchGrabTestOnLegacyClient : public TouchGrabTest {
    virtual void SetUp() {
        SetDevice("tablets/N-Trig-MultiTouch.desc");

        xi2_major_minimum = 2;
        xi2_minor_minimum = 0;

        XITServerInputTest::SetUp();
    }
};

TEST_F(TouchGrabTestOnLegacyClient, ActivePointerGrabUngrabDuringTouch)
{
    XORG_TESTCASE("Create window\n"
                  "Select for button events on XIAllMasterDevices.\n"
                  "Touch begin\n"
                  "Activate async active grab on the window\n"
                  "Touch end, second touch begin\n"
                  "Ungrab device\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=66720");

    ::Display *dpy = Display(); /* XI 2.0 client */

    int deviceid;
    FindInputDeviceByName(dpy, "--device--", &deviceid);

    Window win = CreateWindow(dpy, DefaultRootWindow(dpy));

    SelectXI2Events(dpy, XIAllMasterDevices, win,
                    XI_ButtonPress,
                    XI_ButtonRelease,
                    XI_Enter,
                    XI_Leave,
                    XI_Motion,
                    -1);

    TouchBegin(200, 200);
    TouchUpdate(202, 202);

    EXPECT_EVENT(XIDeviceEvent, m1, dpy, GenericEvent, xi2_opcode, XI_Motion);
    EXPECT_EVENT(XIDeviceEvent, p1, dpy, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_TRUE(NoEventPending(dpy));

    GrabPointer(dpy, VIRTUAL_CORE_POINTER_ID, win);
    TouchEnd();

    EXPECT_EVENT(XIDeviceEvent, r1, dpy, GenericEvent, xi2_opcode, XI_ButtonRelease);

    TouchBegin(200, 200);
    TouchUpdate(202, 202);
    EXPECT_EVENT(XIDeviceEvent, m2, dpy, GenericEvent, xi2_opcode, XI_Motion);
    EXPECT_EVENT(XIDeviceEvent, p2, dpy, GenericEvent, xi2_opcode, XI_ButtonPress);
    XIUngrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, CurrentTime);

    TouchEnd();

    ASSERT_TRUE(NoEventPending(dpy));
}


TEST_F(TouchGrabTestOnLegacyClient, ActivePointerGrabOverPointerSelection)
{
    XORG_TESTCASE("Create window\n"
                  "Select for button press on slave device and XIAllMasterDevices.\n"
                  "Async active grab on the window\n"
                  "Touch begin/end over the window.\n"
                  "Expect button events on slave\n"
                  "Expect button events on master\n");

    ::Display *dpy = Display(); /* XI 2.0 client */

    int deviceid;
    FindInputDeviceByName(dpy, "--device--", &deviceid);

    Window win = CreateWindow(dpy, DefaultRootWindow(dpy));

    XIEventMask mask[2];
    mask[0].deviceid = deviceid;
    mask[0].mask_len = XIMaskLen(XI_TouchOwnership);
    mask[0].mask = new unsigned char[mask[0].mask_len]();

    XISetMask(mask[0].mask, XI_ButtonPress);
    XISetMask(mask[0].mask, XI_ButtonRelease);
    XISetMask(mask[0].mask, XI_Enter);
    XISetMask(mask[0].mask, XI_Leave);
    XISetMask(mask[0].mask, XI_Motion);

    mask[1].deviceid = XIAllMasterDevices;
    mask[1].mask_len = XIMaskLen(XI_TouchOwnership);
    mask[1].mask = new unsigned char[mask[1].mask_len]();

    XISetMask(mask[1].mask, XI_ButtonPress);
    XISetMask(mask[1].mask, XI_ButtonRelease);
    XISetMask(mask[1].mask, XI_Enter);
    XISetMask(mask[1].mask, XI_Leave);
    XISetMask(mask[1].mask, XI_Motion);

    XISelectEvents(dpy, win, mask, 2);
    GrabPointer(dpy, VIRTUAL_CORE_POINTER_ID, win);

    TouchBegin(200, 200);
    TouchUpdate(202, 202);
    TouchEnd();

    EXPECT_EVENT(XIDeviceEvent, press_dev_1, dpy, GenericEvent, xi2_opcode, XI_ButtonPress);
    EXPECT_EQ(press_dev_1->deviceid, deviceid);
    EXPECT_EVENT(XIDeviceEvent, motion, dpy, GenericEvent, xi2_opcode, XI_Motion);
    EXPECT_EQ(motion->deviceid, VIRTUAL_CORE_POINTER_ID);

    EXPECT_EVENT(XIDeviceEvent, press, dpy, GenericEvent, xi2_opcode, XI_ButtonPress);
    EXPECT_EQ(press->deviceid, VIRTUAL_CORE_POINTER_ID);
    EXPECT_EVENT(XIDeviceEvent, release_dev_1, dpy, GenericEvent, xi2_opcode, XI_ButtonRelease);
    EXPECT_EQ(release_dev_1->deviceid, deviceid);
    EXPECT_EVENT(XIDeviceEvent, release, dpy, GenericEvent, xi2_opcode, XI_ButtonRelease);
    EXPECT_EQ(release->deviceid, VIRTUAL_CORE_POINTER_ID);

    ASSERT_TRUE(NoEventPending(dpy));
}


class TouchUngrabTest : public TouchGrabTest,
                        public ::testing::WithParamInterface<enum GrabType>
{
    virtual void SetUp() {
        SetDevice("tablets/N-Trig-MultiTouch.desc");

        xi2_major_minimum = 2;
        switch(GetParam()) {
            case GRABTYPE_XI2TOUCH:
                xi2_minor_minimum = 2;
                break;
            default:
                xi2_minor_minimum = 0;
                break;
        }

        XITServerInputTest::SetUp();
    }
};

TEST_P(TouchUngrabTest, UngrabButtonDuringTouch)
{
    XORG_TESTCASE("Grab a button\n"
                  "Trigger pointer-emulated button grab\n"
                  "Ungrab the button\n"
                  "Continue with touch events\n");
    enum GrabType grab_type = GetParam();
    SCOPED_TRACE("" + grabtype_enum_to_string(grab_type));

    ::Display *dpy = Display();

    XIEventMask mask;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask = new unsigned char[mask.mask_len]();
    if (grab_type == GRABTYPE_XI2) {
        XISetMask(mask.mask, XI_ButtonPress);
        XISetMask(mask.mask, XI_ButtonRelease);
        XISetMask(mask.mask, XI_Motion);
    } else if (grab_type == GRABTYPE_XI2TOUCH) {
        XISetMask(mask.mask, XI_TouchBegin);
        XISetMask(mask.mask, XI_TouchUpdate);
        XISetMask(mask.mask, XI_TouchEnd);
    }

    XIGrabModifiers modifiers = {};
    modifiers.modifiers = XIAnyModifier;

    switch (grab_type) {
        case GRABTYPE_CORE:
            XGrabButton(dpy, AnyButton, XIAnyModifier,
                        DefaultRootWindow(dpy), False,
                        ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
                        GrabModeAsync, GrabModeAsync, None, None);
            break;
        case GRABTYPE_XI1:
            break;
        case GRABTYPE_XI2:
            XIGrabButton(dpy, VIRTUAL_CORE_POINTER_ID,
                         XIAnyButton, DefaultRootWindow(dpy),
                         None, GrabModeAsync, GrabModeAsync, False,
                         &mask, 1, &modifiers);
            break;
        case GRABTYPE_XI2TOUCH:
            XIGrabTouchBegin(dpy, VIRTUAL_CORE_POINTER_ID,
                             DefaultRootWindow(dpy), False,
                             &mask, 1, &modifiers);
            break;
    }

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    switch (grab_type) {
        case GRABTYPE_CORE:
            {
                ASSERT_EVENT(XEvent, press, dpy, ButtonPress);
                XUngrabButton(dpy, AnyButton, AnyModifier, DefaultRootWindow(dpy));
                break;
            }
        case GRABTYPE_XI1:
            break;
        case GRABTYPE_XI2:
            {
                ASSERT_EVENT(XIDeviceEvent, press, dpy, GenericEvent, xi2_opcode, XI_ButtonPress);
                XIUngrabButton(dpy, VIRTUAL_CORE_POINTER_ID, XIAnyButton,
                        DefaultRootWindow(dpy), 1, &modifiers);
                break;
            }
        case GRABTYPE_XI2TOUCH:
            {
                ASSERT_EVENT(XIDeviceEvent, tbegin, dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
                XIUngrabTouchBegin(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy), 1, &modifiers);
            }
    }

    ASSERT_TRUE(NoEventPending(dpy));
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_update.events");

    switch (grab_type) {
        case GRABTYPE_CORE:
            {
                ASSERT_EVENT(XEvent, motion, dpy, MotionNotify);
                break;
            }
        case GRABTYPE_XI1:
            break;
        case GRABTYPE_XI2:
            {
                ASSERT_EVENT(XIDeviceEvent, motion, dpy, GenericEvent, xi2_opcode, XI_Motion);
                break;
            }
        case GRABTYPE_XI2TOUCH:
            {
                ASSERT_EVENT(XIDeviceEvent, tupdate, dpy, GenericEvent, xi2_opcode, XI_TouchUpdate);
                break;
            }
    }

    ASSERT_TRUE(NoEventPending(dpy));
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    switch (grab_type) {
        case GRABTYPE_CORE:
            {
                ASSERT_EVENT(XEvent, release, dpy, ButtonRelease);
                break;
            }
        case GRABTYPE_XI1:
            break;
        case GRABTYPE_XI2:
            {
                ASSERT_EVENT(XIDeviceEvent, release, dpy, GenericEvent,
                             xi2_opcode, XI_ButtonRelease);
                break;
            }
        case GRABTYPE_XI2TOUCH:
            {
                ASSERT_EVENT(XIDeviceEvent, tend, dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
                XIAllowTouchEvents(dpy, tend->deviceid, tend->detail,
                                   DefaultRootWindow(dpy), XIAcceptTouch);
                break;
            }
    }

    delete[] mask.mask;

}

INSTANTIATE_TEST_CASE_P(, TouchUngrabTest,
                        ::testing::Values(GRABTYPE_CORE, GRABTYPE_XI2, GRABTYPE_XI2TOUCH));

/**
 * @tparam AsyncPointer, SyncPointer, ReplayPointer
 */
class TouchGrabTestAllowSome : public TouchGrabTest,
                               public ::testing::WithParamInterface<int> {
};

TEST_P(TouchGrabTestAllowSome, TouchGrabPassedToPassivePassedToRegular)
{
    int mode = GetParam();
    std::string strmode;
    switch(mode) {
        case ReplayPointer:     strmode = "ReplayPointer"; break;
        case AsyncPointer:      strmode = "AsyncPointer"; break;
        case SyncPointer:       strmode = "SyncPointer"; break;
    }

    XORG_TESTCASE("Client 1: register for touch grabs with ownership on root window.\n"
                  "Client 2: register for sync passive core pointer grab on root window.\n"
                  "Client 3: register for core pointer events on root window.\n"
                  "Touch begin/end, repeat\n"
                  "Client 1: reject touch\n"
                  "Client 2: AllowSome(" + strmode + ")\n"
                  "Client 3: consume events.\n"
                  "Repeat\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=56578");

    ::Display *dpy1 = Display();
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    ::Display *dpy3 = XOpenDisplay(server.GetDisplayString().c_str());

    XSynchronize(dpy1, True);
    XSynchronize(dpy2, True);
    XSynchronize(dpy3, True);

    Window root = DefaultRootWindow(dpy1);
    Window win = CreateWindow(dpy2, None);

    XIEventMask mask;
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask_len = XIMaskLen(XI_TouchOwnership);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchOwnership);
    XISetMask(mask.mask, XI_TouchEnd);

    XIGrabModifiers mods = {};
    mods.modifiers = XIAnyModifier;
    ASSERT_EQ(Success, XIGrabTouchBegin(dpy1, VIRTUAL_CORE_POINTER_ID,
                                        root, False, &mask, 1, &mods));
    delete[] mask.mask;

    XGrabButton(dpy2, AnyButton, XIAnyModifier, win, False,
                ButtonPressMask|ButtonReleaseMask,
                GrabModeSync, GrabModeAsync, None, None);

    XSelectInput(dpy3, win, ButtonPressMask|ButtonReleaseMask);

    for (int i = 0; i < 2; i++) {
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_update.events");
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");
    }

    for (int i = 0; i < 2; i++) {
        ASSERT_EVENT(XIDeviceEvent, tbegin, dpy1, GenericEvent, xi2_opcode, XI_TouchBegin);
        ASSERT_EVENT(XIDeviceEvent, towner, dpy1, GenericEvent, xi2_opcode, XI_TouchOwnership);
        ASSERT_EVENT(XIDeviceEvent, tupdate, dpy1, GenericEvent, xi2_opcode, XI_TouchUpdate);
        ASSERT_EVENT(XIDeviceEvent, tend, dpy1, GenericEvent, xi2_opcode, XI_TouchEnd);
        XIAllowTouchEvents(dpy1, tbegin->deviceid, tbegin->detail, root, XIRejectTouch);
        if (i == 0)
            ASSERT_EQ(XPending(dpy1), 4); /* begin, owner, update, end of
                                             second touch */
        else
            ASSERT_TRUE(NoEventPending(dpy1));

        ASSERT_EVENT(XEvent, grab_press, dpy2, ButtonPress);
        XAllowEvents(dpy2, mode, CurrentTime);

        if (mode == ReplayPointer) {
            ASSERT_EVENT(XEvent, press, dpy3, ButtonPress);
            ASSERT_EVENT(XEvent, release, dpy3, ButtonRelease);
            ASSERT_TRUE(NoEventPending(dpy2));
        } else {
            ASSERT_EVENT(XEvent, release, dpy2, ButtonRelease);
            ASSERT_TRUE(NoEventPending(dpy3));
        }
    }
}

INSTANTIATE_TEST_CASE_P(, TouchGrabTestAllowSome,
                        ::testing::Values(AsyncPointer, SyncPointer, ReplayPointer));

class TouchGrabTestMultipleModes : public TouchGrabTest,
                                   public ::testing::WithParamInterface<int>
{
};

TEST_P(TouchGrabTestMultipleModes, SingleTouchGrabListenerAcceptRejectBeforeTouchEnd)
{
    int mode = GetParam();
    std::string strmode = (mode == XIAcceptTouch) ? "XIAcceptTouch" : "XIRejectTouch";

    XORG_TESTCASE("Register for a touch grab.\n"
                  "Begin touch\n"
                  "Verify begin event is received.\n"
                  "Call XIAllowTouchEvents(i" + strmode + ")\n"
                  "End touch\n"
                  "Verify end event is received at the right time.\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);

    XIEventMask mask;
    mask.deviceid = VIRTUAL_CORE_POINTER_ID;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    XIGrabModifiers mods = {};
    mods.modifiers = XIAnyModifier;
    ASSERT_EQ(Success, XIGrabTouchBegin(dpy, VIRTUAL_CORE_POINTER_ID,
                                        root, False, &mask, 1, &mods));
    delete[] mask.mask;

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    ASSERT_EVENT(XIDeviceEvent, tbegin, dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
    XIAllowTouchEvents(dpy, tbegin->deviceid, tbegin->detail, root, mode);

    if (mode == XIAcceptTouch)
        ASSERT_TRUE(NoEventPending(dpy));
    else {
        ASSERT_EVENT(XIDeviceEvent, tend, dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
    }

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    if (mode == XIAcceptTouch) {
        ASSERT_EVENT(XIDeviceEvent, tend, dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
    }

    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_P(TouchGrabTestMultipleModes, ActiveAndPassiveGrab)
{
    int mode = GetParam();
    std::string strmode = (mode == XIAcceptTouch) ? "XIAcceptTouch" : "XIRejectTouch";
    XORG_TESTCASE("Grab button 1 on device\n"
                  "Actively grab device\n"
                  "Generate touch begin/end on the device\n"
                  "XAllowTouch(" + strmode + ") on TouchBegin\n"
                  "Expect one begin/end pair for XIAcceptTouch,\n"
                  "two for XIRejectTouch\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=55738");

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    XIEventMask mask;
    mask.deviceid = deviceid;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    XIGrabModifiers modifier = {0};
    modifier.modifiers = XIAnyModifier;

    Window win = XDefaultRootWindow(Display());

    XIGrabButton(Display(), deviceid, 1, win, None, GrabModeAsync, GrabModeAsync, False, &mask, 1, &modifier);
    ASSERT_EQ(XIGrabTouchBegin(Display(), deviceid, win, False, &mask, 1, &modifier), Success);
    XIGrabDevice(Display(), deviceid, win, CurrentTime, None, GrabModeAsync, GrabModeAsync, False, &mask);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    XSync(Display(), False);

    int tbegin_received = 0;
    int tend_received = 0;

    while (XPending(Display())) {
        XEvent ev;
        XNextEvent(Display(), &ev);

        if (ev.xcookie.type == GenericEvent &&
            ev.xcookie.extension == xi2_opcode &&
            XGetEventData(Display(), &ev.xcookie)) {
            XIDeviceEvent *event = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);
            switch(event->evtype) {
                case XI_TouchBegin:
                    tbegin_received++;
                    XIAllowTouchEvents(Display(), event->deviceid, event->detail, win, mode);
                    break;
                case XI_TouchEnd:
                    tend_received++;
                    break;
            }
        }

        XFreeEventData(Display(), &ev.xcookie);
        XSync(Display(), False);
    }
    ASSERT_FALSE(xorg::testing::XServer::WaitForEventOfType(Display(),
                GenericEvent,
                xi2_opcode,
                XI_TouchEnd,
                500));
    if (mode == XIAcceptTouch) {
        ASSERT_EQ(tbegin_received, 1);
        ASSERT_EQ(tend_received, 1);
    } else {
        ASSERT_EQ(tbegin_received, 2);
        ASSERT_EQ(tend_received, 2);
    }

    delete[] mask.mask;
}

INSTANTIATE_TEST_CASE_P(, TouchGrabTestMultipleModes, ::testing::Values(XIAcceptTouch, XIRejectTouch));

class TouchGrabTestMultipleTaps : public TouchGrabTest,
                                  public ::testing::WithParamInterface<std::tr1::tuple<int, int> > {};

TEST_P(TouchGrabTestMultipleTaps, PassiveGrabPointerEmulationMultipleTouchesFastSuccession)
{
    std::tr1::tuple<int, int> t = GetParam();
    int repeats = std::tr1::get<0>(t);
    int mode = std::tr1::get<1>(t);

    std::string strmode = (mode == GrabModeAsync) ? "GrabModeAsync" : "GrabModeSync";
    std::stringstream ss;
    ss << repeats;

    XORG_TESTCASE("Grab button 1 (" + strmode + ") from client C1\n"
                  "Select for core button events from client C2\n"
                  "Generate " + ss.str() + " touch begin/end events\n"
                  "C2 must not receive button events\n"
                  "This test exceeds num_touches on the device.\n");

    ::Display *dpy1 = Display();

    XIEventMask mask;
    mask.deviceid = XIAllDevices;
    mask.mask_len = XIMaskLen(XI_ButtonRelease);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_ButtonPress);
    XISetMask(mask.mask, XI_ButtonRelease);

    XIGrabModifiers mods;
    mods.modifiers = XIAnyModifier;
    XIGrabButton(dpy1, VIRTUAL_CORE_POINTER_ID, 1, DefaultRootWindow(dpy1),
                 None, mode, mode, False, &mask, 1, &mods);
    delete[] mask.mask;

    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy2);
    Window root = DefaultRootWindow(dpy2);

    XSelectInput(dpy2, root, PointerMotionMask | ButtonPressMask | ButtonReleaseMask);
    XSync(dpy2, False);
    ASSERT_TRUE(NoEventPending(dpy2));

    /* for this test to succeed, the server mustn't start processing until
       the last touch event physically ends, so no usleep here*/
    for (int i = 0; i < repeats; i++) {
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");
    }

    XSync(dpy1, False);
    XSync(dpy2, False);

    ASSERT_GT(XPending(dpy1), 0);
    int event_count = 0;
    while (XPending(dpy1)) {
        event_count++;

        ASSERT_EVENT(XIDeviceEvent, press, dpy1, GenericEvent, xi2_opcode, XI_ButtonPress);
        if (mode == GrabModeSync)
            XIAllowEvents(dpy1, press->deviceid, XISyncDevice, CurrentTime);
        ASSERT_EVENT(XIDeviceEvent, release, dpy1, GenericEvent, xi2_opcode, XI_ButtonRelease);
    }

    ASSERT_EQ(event_count, repeats); // ButtonPress event

    while (XPending(dpy2)) {
        ASSERT_EVENT(XIDeviceEvent, ev, dpy2, MotionNotify);
    }
}

TEST_P(TouchGrabTestMultipleTaps, PassiveGrabPointerRelease)
{
    std::tr1::tuple<int, int> param = GetParam();
    int repeats = std::tr1::get<0>(param);
    int mode = std::tr1::get<1>(param);

    std::string strmode = (mode == GrabModeAsync) ? "GrabModeAsync" : "GrabModeSync";
    std::stringstream ss;
    ss << repeats;

    XORG_TESTCASE("Grab button 1 (" + strmode + ") from client C1\n"
                  "Select for touch events from client C2\n"
                  "Generate " + ss.str() + " touch begin/end events\n"
                  "C2 must not get touch events\n"
                  "This test exceeds num_touches on the device.\n");

    ::Display *dpy1 = Display();

    XIEventMask mask;
    mask.deviceid = XIAllDevices;
    mask.mask_len = XIMaskLen(XI_ButtonRelease);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_ButtonPress);

    XIGrabModifiers mods;
    mods.modifiers = XIAnyModifier;
    XIGrabButton(dpy1, VIRTUAL_CORE_POINTER_ID, 1, DefaultRootWindow(dpy1),
                 None, mode, mode, False, &mask, 1, &mods);
    delete[] mask.mask;

    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy2);
    Window root = DefaultRootWindow(dpy2);

    int major = 2, minor = 2;
    ASSERT_EQ(XIQueryVersion(dpy2, &major, &minor), Success);

    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    XISelectEvents(dpy2, root, &mask, 1);
    delete[] mask.mask;

    XSync(dpy2, False);
    ASSERT_TRUE(NoEventPending(dpy2));

    /* for this test to succeed, the server mustn't start processing until
       the last touch event physically ends, so no usleep here*/
    for (int i = 0; i < repeats; i++) {
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");
    }

    XSync(dpy1, False);
    XSync(dpy2, False);

    ASSERT_TRUE(XPending(dpy1));
    int event_count = 0;
    while (XPending(dpy1)) {
        event_count++;

        ASSERT_EVENT(XIDeviceEvent, ev, dpy1, GenericEvent, xi2_opcode, XI_ButtonPress);
        if (mode == GrabModeSync)
            XIAllowEvents(dpy1, ev->deviceid, XISyncDevice, CurrentTime);
    }

    ASSERT_EQ(event_count, repeats); // ButtonPress event

    ASSERT_TRUE(NoEventPending(dpy2));
}

INSTANTIATE_TEST_CASE_P(, TouchGrabTestMultipleTaps,
        ::testing::Combine(
            ::testing::Range(1, 11), /* device num_touches is 5 */
            ::testing::Values(GrabModeSync, GrabModeAsync)
        )
);

class TouchOwnershipTest : public TouchGrabTest {
public:
    void GrabTouchOnWindow(::Display *dpy, Window win, bool ownership = false)
    {
        XIEventMask mask;
        mask.deviceid = XIAllMasterDevices;
        mask.mask_len = XIMaskLen(XI_TouchOwnership);
        mask.mask = new unsigned char[mask.mask_len]();

        XISetMask(mask.mask, XI_TouchBegin);
        XISetMask(mask.mask, XI_TouchEnd);
        XISetMask(mask.mask, XI_TouchUpdate);

        if (ownership)
            XISetMask(mask.mask, XI_TouchOwnership);
        XIGrabModifiers mods = {};
        mods.modifiers = XIAnyModifier;
        ASSERT_EQ(Success, XIGrabTouchBegin(dpy, XIAllMasterDevices,
                           win, False, &mask, 1, &mods));

        delete[] mask.mask;
        XSync(dpy, False);
    }

    void SelectTouchOnWindow(::Display *dpy, Window win, bool ownership = false)
    {
        XIEventMask mask;
        mask.deviceid = XIAllMasterDevices;
        mask.mask_len = XIMaskLen(XI_TouchOwnership);
        mask.mask = new unsigned char[mask.mask_len]();

        XISetMask(mask.mask, XI_TouchBegin);
        XISetMask(mask.mask, XI_TouchEnd);
        XISetMask(mask.mask, XI_TouchUpdate);

        if (ownership)
            XISetMask(mask.mask, XI_TouchOwnership);
        XISelectEvents(dpy, win, &mask, 1);

        delete[] mask.mask;
        XSync(dpy, False);
    }

};

TEST_F(TouchOwnershipTest, OwnershipAfterRejectTouch)
{
    XORG_TESTCASE("Client 1 registers a touch grab on the root window.\n"
                  "Clients 2-4 register a touch grab with TouchOwnership on respective subwindows.\n"
                  "Client 5 selects for TouchOwnership events on last window.\n"
                  "TouchBegin/End in the window.\n"
                  "Expect a TouchBegin to all clients.\n"
                  "Reject touch one-by-one in all clients\n"
                  "Expect TouchOwnership to traverse down\n");

    const int NCLIENTS = 5;
    ::Display *dpys[NCLIENTS];
    dpys[0] = Display();
    XSynchronize(dpys[0], True);
    for (int i = 1; i < NCLIENTS; i++)
        dpys[i] = NewClient();

    Window win[NCLIENTS];
    win[0] = DefaultRootWindow(dpys[0]);

    /* Client A has a touch grab on the root window */
    GrabTouchOnWindow(dpys[0], DefaultRootWindow(dpys[0]));
    for (int i = 1; i < NCLIENTS - 1; i++) {
        /* other clients selects with ownership*/
        Window w = CreateWindow(dpys[i], win[i-1]);
        GrabTouchOnWindow(dpys[i], w, true);
        win[i] = w;
    }

    /* last client has selection only */
    Window w = CreateWindow(dpys[NCLIENTS - 1], win[NCLIENTS-2]);
    win[NCLIENTS-1] = w;
    SelectTouchOnWindow(dpys[NCLIENTS-1], w, true);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    int touchid = -1;

    for (int i = 0; i < NCLIENTS; i++) {
        /* Expect touch begin on all clients */
        ASSERT_EVENT(XIDeviceEvent, tbegin, dpys[i], GenericEvent, xi2_opcode, XI_TouchBegin);
        if (touchid == -1)
            touchid = tbegin->detail;
        else
            ASSERT_EQ(touchid, tbegin->detail);

        /* A has no ownership mask, everyone else doesn't have ownership
         * yet, so everyone only has the TouchBegin so far*/
        ASSERT_TRUE(NoEventPending(dpys[i]));
    }

    /* Now reject one-by-one */
    for (int i = 1; i < NCLIENTS; i++) {
        int current_owner = i-1;
        int next_owner = i;
        XIAllowTouchEvents(dpys[current_owner], VIRTUAL_CORE_POINTER_ID, touchid, win[current_owner], XIRejectTouch);

        ASSERT_EVENT(XIDeviceEvent, tend, dpys[current_owner], GenericEvent, xi2_opcode, XI_TouchEnd);
        ASSERT_TRUE(NoEventPending(dpys[current_owner]));

        /* Now we expect ownership */
        ASSERT_EQ(XPending(dpys[next_owner]), 1);
        ASSERT_EVENT(XITouchOwnershipEvent, oe, dpys[next_owner], GenericEvent, xi2_opcode, XI_TouchOwnership);
        ASSERT_EQ(oe->touchid, (unsigned int)touchid);
    }

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    int last_owner = NCLIENTS-1;
    ASSERT_EVENT(XIDeviceEvent, tend, dpys[last_owner], GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_EQ(tend->detail, touchid);

    for (int i = 0; i < NCLIENTS; i++)
        ASSERT_TRUE(NoEventPending(dpys[i]));
}

TEST_F(TouchOwnershipTest, NoOwnershipAfterAcceptTouch)
{
    XORG_TESTCASE("Create a window W.\n"
                  "Client A registers a touch grab on the root window.\n"
                  "Client B register for TouchOwnership events on W.\n"
                  "TouchBegin/End in the window.\n"
                  "Expect a TouchBegin to B\n"
                  "Accept touch in A\n"
                  "Expect TouchEnd in B, no ownership\n");
    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    ::Display *dpy2 = NewClient();

    /* Client A has a touch grab on the root window */
    GrabTouchOnWindow(dpy, DefaultRootWindow(dpy));

    /* CLient B selects for events on window */
    Window win = CreateWindow(dpy2, DefaultRootWindow(dpy));
    SelectTouchOnWindow(dpy2, win, true);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    /* Expect touch begin to A */
    ASSERT_EVENT(XIDeviceEvent, A_begin, dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
    int touchid = A_begin->detail;
    int deviceid = A_begin->deviceid;
    Window root = A_begin->root;


    /* Now expect the touch begin to B */
    ASSERT_EVENT(XIDeviceEvent, B_begin, dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_EQ(B_begin->detail, touchid);

    /* A has not rejected yet, no ownership */
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowTouchEvents(dpy, deviceid, touchid, root, XIAcceptTouch);

    /* Now we expect TouchEnd on B */
    ASSERT_EQ(XPending(dpy2), 1);
    ASSERT_EVENT(XIDeviceEvent, B_end, dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_EQ(B_end->detail, touchid);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    ASSERT_EVENT(XIDeviceEvent, A_end, dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_EQ(A_end->detail, touchid);

    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_F(TouchOwnershipTest, ActiveGrabOwnershipAcceptTouch)
{
    XORG_TESTCASE("Client A actively grabs the device.\n"
                  "Client B register for TouchOwnership events on the root window.\n"
                  "TouchBegin/End in the window.\n"
                  "Expect a TouchBegin to A and B\n"
                  "Accept touch in A\n"
                  "Expect TouchEnd in B, no ownership\n");
    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    ::Display *dpy2 = NewClient();

    /* CLient B selects for events on window */
    Window win = CreateWindow(dpy2, DefaultRootWindow(dpy));
    SelectTouchOnWindow(dpy2, win, true);

    /* Client A grabs the device */
    GrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    /* Expect touch begin to A */
    ASSERT_EVENT(XIDeviceEvent, A_begin, dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
    int touchid = A_begin->detail;
    int deviceid = A_begin->deviceid;
    Window root = A_begin->root;

    /* Now expect the touch begin to B */
    ASSERT_EVENT(XIDeviceEvent, B_begin, dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_EQ(B_begin->detail, touchid);

    /* A has not rejected yet, no ownership */
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowTouchEvents(dpy, deviceid, touchid, root, XIAcceptTouch);

    /* Now we expect TouchEnd */
    ASSERT_EQ(XPending(dpy2), 1);
    ASSERT_EVENT(XIDeviceEvent, B_end, dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_EQ(B_end->detail, touchid);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    ASSERT_EVENT(XIDeviceEvent, A_end, dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_EQ(A_end->detail, touchid);

    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_F(TouchOwnershipTest, ActiveGrabOwnershipRejectTouch)
{
    XORG_TESTCASE("Client A actively grabs the device.\n"
                  "Client B register for TouchOwnership events on the root window.\n"
                  "TouchBegin/End in the window.\n"
                  "Expect a TouchBegin to A and B\n"
                  "Accept touch in A\n"
                  "Expect TouchEnd in B, no ownership\n");
    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    ::Display *dpy2 = NewClient();

    /* CLient B selects for events on window */
    Window win = CreateWindow(dpy2, DefaultRootWindow(dpy));
    SelectTouchOnWindow(dpy2, win, true);

    /* Client A grabs the device */
    GrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    /* Expect touch begin to A */
    ASSERT_EVENT(XIDeviceEvent, A_begin, dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
    int touchid = A_begin->detail;
    int deviceid = A_begin->deviceid;
    Window root = A_begin->root;

    /* Now expect the touch begin to B */
    ASSERT_EVENT(XIDeviceEvent, B_begin, dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_EQ(B_begin->detail, touchid);

    /* A has not rejected yet, no ownership */
    ASSERT_TRUE(NoEventPending(dpy2));

    XIAllowTouchEvents(dpy, deviceid, touchid, root, XIRejectTouch);

    ASSERT_EVENT(XIDeviceEvent, A_end, dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_EQ(A_end->detail, touchid);

    /* Now we expect TouchOwnership */
    ASSERT_EQ(XPending(dpy2), 1);

    ASSERT_EVENT(XITouchOwnershipEvent, oe, dpy2, GenericEvent, xi2_opcode, XI_TouchOwnership);
    ASSERT_EQ(oe->touchid, (unsigned int)touchid);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    ASSERT_EVENT(XIDeviceEvent, B_end, dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);

    ASSERT_TRUE(NoEventPending(dpy2));
}

TEST_F(TouchOwnershipTest, ActiveGrabOwnershipUngrabDevice)
{
    XORG_TESTCASE("Client A actively grabs the device.\n"
                  "Client B register for TouchOwnership events on the root window.\n"
                  "TouchBegin/End in the window.\n"
                  "Expect a TouchBegin to A and B\n"
                  "Accept touch in A\n"
                  "Expect TouchEnd in B, no ownership\n");
    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    ::Display *dpy2 = NewClient();

    /* CLient B selects for events on window */
    Window win = CreateWindow(dpy2, DefaultRootWindow(dpy));
    SelectTouchOnWindow(dpy2, win, true);

    /* Client A grabs the device */
    GrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    /* Expect touch begin to A */
    ASSERT_EVENT(XIDeviceEvent, A_begin, dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
    int touchid = A_begin->detail;
    int deviceid = A_begin->deviceid;

    /* Now expect the touch begin to B */
    ASSERT_EVENT(XIDeviceEvent, B_begin, dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_EQ(B_begin->detail, touchid);

    /* A has not rejected yet, no ownership */
    ASSERT_TRUE(NoEventPending(dpy2));

    XIUngrabDevice(dpy, deviceid, CurrentTime);

    /* A needs TouchEnd */
    ASSERT_EVENT(XIDeviceEvent, A_end, dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_EQ(A_end->detail, touchid);

    /* Now we expect TouchOwnership on B */
    ASSERT_EVENT(XITouchOwnershipEvent, oe, dpy2, GenericEvent, xi2_opcode, XI_TouchOwnership);
    ASSERT_EQ(oe->touchid, (unsigned int)touchid);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    ASSERT_EVENT(XIDeviceEvent, B_end, dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_EQ(B_end->detail, touchid);

    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_F(TouchOwnershipTest, ActivePointerGrabForWholeTouch)
{
    XORG_TESTCASE("Client A pointer-grabs the device.\n"
                  "Client B registers for TouchOwnership events on a window.\n"
                  "TouchBegin/End in the window.\n"
                  "Expect ButtonPress/Release to A.\n"
                  "Expect TouchBegin/End to B without ownership.");

    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    ::Display *dpy2 = NewClient();

    /* CLient B selects for events on window */
    Window win = CreateWindow(dpy2, DefaultRootWindow(dpy));
    SelectTouchOnWindow(dpy2, win, true);

    /* Client A grabs the device */
    GrabPointer(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    ASSERT_EVENT(XIDeviceEvent, A_btnpress, dpy, GenericEvent, xi2_opcode, XI_ButtonPress);

    ASSERT_EVENT(XIDeviceEvent, B_begin, dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);

    /* No ownership event on the wire */
    ASSERT_TRUE(NoEventPending(dpy2));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    ASSERT_EVENT(XIDeviceEvent, A_btnrelease, dpy, GenericEvent, xi2_opcode, XI_ButtonRelease);

    /* One event on the wire and it's TouchEnd, not ownership */
    ASSERT_EQ(XPending(dpy2), 1);

    XIUngrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, CurrentTime);

    ASSERT_EVENT(XIDeviceEvent, B_end, dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);

    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_F(TouchOwnershipTest, ActivePointerUngrabDuringTouch)
{
    XORG_TESTCASE("Client A pointer-grabs the device.\n"
                  "Client B registers for TouchOwnership events on a window.\n"
                  "TouchBegin in the window.\n"
                  "Expect ButtonPress to A.\n"
                  "Ungrab pointer.\n"
                  "TouchEnd to B.\n");

    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    ::Display *dpy2 = NewClient();

    /* CLient B selects for events on window */
    Window win = CreateWindow(dpy2, DefaultRootWindow(dpy));
    SelectTouchOnWindow(dpy2, win, true);

    /* Client A grabs the device */
    GrabPointer(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    ASSERT_EVENT(XIDeviceEvent, A_btnpress, dpy, GenericEvent, xi2_opcode, XI_ButtonPress);

    ASSERT_EVENT(XIDeviceEvent, B_begin, dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);

    /* No ownership event on the wire */
    ASSERT_TRUE(NoEventPending(dpy2));

    XIUngrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, CurrentTime);
    ASSERT_TRUE(NoEventPending(dpy));

    ASSERT_EVENT(XIDeviceEvent, B_end, dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    ASSERT_TRUE(NoEventPending(dpy));
    ASSERT_TRUE(NoEventPending(dpy2));
}

#endif /* HAVE_XI22 */

class GrabModeTest : public XITServerInputTest,
                     public ::testing::WithParamInterface<int>
{
public:
    std::auto_ptr<xorg::testing::evemu::Device> ptr;
    std::auto_ptr<xorg::testing::evemu::Device> kbd;

    virtual void SetUp() {
        ptr = std::auto_ptr<xorg::testing::evemu::Device>(
               new xorg::testing::evemu::Device(
                   RECORDINGS_DIR "mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc"
               ));
        kbd = std::auto_ptr<xorg::testing::evemu::Device>(
               new xorg::testing::evemu::Device(
                   RECORDINGS_DIR "keyboards/AT-Translated-Set-2-Keyboard.desc"
               ));

        XITServerInputTest::SetUp();
    }

    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--pointer--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n"
                               "Option \"Device\" \"" + ptr->GetDeviceNode() + "\"");
        config.AddInputSection("evdev", "--keyboard--",
                               "Option \"CoreKeyboard\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n"
                               "Option \"Device\" \"" + kbd->GetDeviceNode() + "\"");
        config.WriteConfig();
    }
};

TEST_P(GrabModeTest, PointerSyncKeyboardAsync)
{
    XORG_TESTCASE("XI2 grab on the device with sync device, async paired device\n"
                  "play pointer event and keyboard events\n"
                  "expect only the async device to send events\n")

    ::Display *dpy = Display();

    int deviceid = GetParam();

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_ButtonPress);
    XISetMask(mask.mask, XI_ButtonRelease);
    XISetMask(mask.mask, XI_KeyPress);
    XISetMask(mask.mask, XI_KeyRelease);
    XISelectEvents(dpy, DefaultRootWindow(dpy), &mask, 1);

    XIGrabDevice(dpy, deviceid, DefaultRootWindow(dpy),
                 CurrentTime, None, GrabModeSync, GrabModeAsync, False, &mask);

    ptr->PlayOne(EV_KEY, BTN_LEFT, 1, true);
    ptr->PlayOne(EV_KEY, BTN_LEFT, 0, true);

    kbd->PlayOne(EV_KEY, KEY_A, 1, true);
    kbd->PlayOne(EV_KEY, KEY_A, 0, true);

    /* one device is frozen, we expect no events */
    if (deviceid == VIRTUAL_CORE_POINTER_ID) {
        ASSERT_EVENT(XIDeviceEvent, keypress, dpy, GenericEvent, xi2_opcode, XI_KeyPress);
        ASSERT_EVENT(XIDeviceEvent, keyrelease, dpy, GenericEvent, xi2_opcode, XI_KeyRelease);
        ASSERT_TRUE(NoEventPending(dpy));
    } else {
        ASSERT_EVENT(XIDeviceEvent, btnpress, dpy, GenericEvent, xi2_opcode, XI_ButtonPress);
        ASSERT_EVENT(XIDeviceEvent, btnrelease, dpy, GenericEvent, xi2_opcode, XI_ButtonRelease);
        ASSERT_TRUE(NoEventPending(dpy));
    }

    delete[] mask.mask;
}

INSTANTIATE_TEST_CASE_P(, GrabModeTest, ::testing::Values(VIRTUAL_CORE_POINTER_ID, VIRTUAL_CORE_KEYBOARD_ID));
