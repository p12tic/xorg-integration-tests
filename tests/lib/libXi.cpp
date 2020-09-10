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

#include <linux/input.h>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/extensions/XI2.h>
#include <X11/extensions/XInput2.h>

#include <xit-server-input-test.h>
#include <xit-property.h>
#include <device-emulated-interface.h>

#include "helpers.h"
#include "xit-event.h"

/**
 * Test for libXi-related bugs. Initialises a single emulated pointer device.
 */
class libXiTest : public XITServerInputTest,
                  public DeviceEmulatedInterface {
public:
    /**
     * Initializes a standard mouse device with two wheels.
     */
    virtual void SetUp() {
        AddDevice(xorg::testing::emulated::DeviceType::POINTER);
        AddDevice(xorg::testing::emulated::DeviceType::KEYBOARD);
        XITServerInputTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CorePointer device based on
     * the evemu device.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("emulated", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n" +
                               Dev(0).GetOptions());
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("emulated", "--keyboard-device--",
                               "Option \"CoreKeyboard\" \"on\"\n" +
                               Dev(1).GetOptions());
        config.WriteConfig();
    }


    void StartServer() override {
        XITServerInputTest::StartServer();
        WaitOpen();
    }
};

TEST_F(libXiTest, DisplayNotGarbage)
{
    XORG_TESTCASE("https://bugzilla.redhat.com/show_bug.cgi?id=804907");

    ::Display *dpy = Display();
    XIEventMask mask;
    unsigned char data[XIMaskLen(XI_LASTEVENT)] = { 0 };

    mask.deviceid = XIAllDevices;
    mask.mask = data;
    mask.mask_len = sizeof(data);
    XISetMask(mask.mask, XI_RawMotion);
    XISetMask(mask.mask, XI_RawButtonPress);
    XISetMask(mask.mask, XI_RawButtonRelease);

    XISelectEvents(dpy, DefaultRootWindow(dpy), &mask, 1);
    XSync(dpy, False);

    Dev(0).PlayRelMotion(-1, 0);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_RawMotion,
                                                           1000));
    XEvent ev;
    XIDeviceEvent *dev;

    XNextEvent(dpy, &ev);
    assert(ev.xcookie.display == dpy);
    assert(XGetEventData(dpy, &ev.xcookie));

    dev = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);
    ASSERT_EQ(dev->display, dpy);

    XFreeEventData(dpy, &ev.xcookie);
}

TEST_F(libXiTest, SerialNumberNotGarbage)
{
    XORG_TESTCASE("https://bugs.fredesktop.org/id=64687");

    ::Display *dpy = Display();


    /* XIDeviceEvent */
    SelectXI2Events(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy),
                    XI_Motion, -1);
    Dev(0).PlayRelMotion(-1, 0);
    Dev(0).PlayRelMotion(-1, 0);

    ASSERT_EVENT(XIDeviceEvent, motion1, dpy, GenericEvent, xi2_opcode, XI_Motion);
    ASSERT_EVENT(XIDeviceEvent, motion2, dpy, GenericEvent, xi2_opcode, XI_Motion);
    EXPECT_EQ(motion1->serial, motion2->serial);

    XSync(dpy, True);

    Dev(0).PlayRelMotion(-1, 0);

    ASSERT_EVENT(XIDeviceEvent, motion3, dpy, GenericEvent, xi2_opcode, XI_Motion);
    EXPECT_LT(motion2->serial, motion3->serial);

    /* XIRawEvent */
    XSync(dpy, True);
    SelectXI2Events(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy),
                    XI_RawMotion, -1);

    Dev(0).PlayRelMotion(-1, 0);
    Dev(0).PlayRelMotion(-1, 0);

    ASSERT_EVENT(XIDeviceEvent, raw1, dpy, GenericEvent, xi2_opcode, XI_RawMotion);
    ASSERT_EVENT(XIDeviceEvent, raw2, dpy, GenericEvent, xi2_opcode, XI_RawMotion);
    EXPECT_GT(raw2->serial, motion3->serial);
    EXPECT_EQ(raw2->serial, raw2->serial);

    XSync(dpy, True);

    Dev(0).PlayRelMotion(-1, 0);

    ASSERT_EVENT(XIDeviceEvent, raw3, dpy, GenericEvent, xi2_opcode, XI_RawMotion);
    EXPECT_LT(raw2->serial, raw3->serial);

    /* XIPropertyEvent, XIHierarchyEvent */
    XSync(dpy, True);
    SelectXI2Events(dpy, XIAllDevices, DefaultRootWindow(dpy),
                    XI_HierarchyChanged, XI_PropertyEvent, -1);

    int deviceid;
    FindInputDeviceByName(dpy, "--device--", &deviceid);
    XITProperty<int> prop(dpy, deviceid, "Device Enabled");
    prop.data[0] = 0;
    prop.Update();

    ASSERT_EVENT(XIPropertyEvent, propev, dpy, GenericEvent, xi2_opcode, XI_PropertyEvent);
    ASSERT_EVENT(XIHierarchyEvent, hierarchy, dpy, GenericEvent, xi2_opcode, XI_HierarchyChanged);
    EXPECT_GT(propev->serial, raw3->serial);
    EXPECT_EQ(hierarchy->serial, propev->serial);

    prop.data[0] = 1;
    prop.Update();
    XSync(dpy, True);

    /* FIXME: should check other events too */
}

#if HAVE_XI22
class libXiTouchTest : public XITServerInputTest,
                       public DeviceEmulatedInterface {
public:
    /**
     * Initializes a touchpad device
     */
    virtual void SetUp() {
        AddDevice(xorg::testing::emulated::DeviceType::TOUCH);
        AddDevice(xorg::testing::emulated::DeviceType::KEYBOARD);
        XITServerInputTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CorePointer device based on
     * the evemu device.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("emulated", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n" +
                               Dev(0).GetOptions());
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("emulated", "--keyboard-device--",
                               "Option \"CoreKeyboard\" \"on\"\n" +
                               Dev(1).GetOptions());
        config.WriteConfig();
    }


    void StartServer() override {
        XITServerInputTest::StartServer();
        WaitOpen();
    }
};


TEST_F(libXiTouchTest, CopyRawTouchEvent)
{
    XORG_TESTCASE("Generate touch begin/end event\n"
                  "Trigger CopyEventCookie through XPeekEvent.\n"
                  "Ensure value is not garbage\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=59491");

    ::Display *dpy = Display();

    XIEventMask mask;
    unsigned char data[XIMaskLen(XI_LASTEVENT)] = { 0 };

    mask.deviceid = XIAllMasterDevices;
    mask.mask = data;
    mask.mask_len = sizeof(data);
    XISetMask(mask.mask, XI_RawTouchBegin);
    XISetMask(mask.mask, XI_RawTouchUpdate);
    XISetMask(mask.mask, XI_RawTouchEnd);

    XISelectEvents(dpy, DefaultRootWindow(dpy), &mask, 1);
    XSync(dpy, False);

    Dev(0).PlayTouchBegin(200, 200, 0);
    Dev(0).PlayTouchUpdate(210, 210, 0);
    Dev(0).PlayTouchEnd(210, 210, 0);

    XSync(dpy, False);
    ASSERT_GT(XPending(dpy), 0);

    XEvent ev = {0};

    ASSERT_TRUE(XPeekEvent(dpy, &ev));
    ASSERT_EQ(ev.xcookie.extension, xi2_opcode);
    ASSERT_EQ(ev.xcookie.evtype, XI_RawTouchBegin);
    ASSERT_GT(ev.xcookie.cookie, 0U);
    ASSERT_TRUE(XGetEventData(dpy, &ev.xcookie));
    XFreeEventData(dpy, &ev.xcookie);
    ASSERT_EVENT(XIRawEvent, begin, dpy, GenericEvent, xi2_opcode, XI_RawTouchBegin);

    ASSERT_TRUE(XPeekEvent(dpy, &ev));
    ASSERT_EQ(ev.xcookie.extension, xi2_opcode);
    ASSERT_EQ(ev.xcookie.evtype, XI_RawTouchUpdate);
    ASSERT_GT(ev.xcookie.cookie, 0U);
    ASSERT_TRUE(XGetEventData(dpy, &ev.xcookie));
    ASSERT_EVENT(XIRawEvent, update, dpy, GenericEvent, xi2_opcode, XI_RawTouchUpdate);

    ASSERT_TRUE(XPeekEvent(dpy, &ev));
    ASSERT_EQ(ev.xcookie.extension, xi2_opcode);
    ASSERT_EQ(ev.xcookie.evtype, XI_RawTouchEnd);
    ASSERT_GT(ev.xcookie.cookie, 0U);
    ASSERT_TRUE(XGetEventData(dpy, &ev.xcookie));
    ASSERT_EVENT(XIRawEvent, end, dpy, GenericEvent, xi2_opcode, XI_RawTouchEnd);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#endif
