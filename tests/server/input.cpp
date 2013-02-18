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

#include <math.h>
#include <pixman.h>

#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>

#include "xit-server-input-test.h"
#include "xit-event.h"
#include "device-interface.h"
#include "helpers.h"

using namespace xorg::testing;

class PointerMotionTest : public XITServerInputTest,
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

enum device_type {
    MOUSE, TABLET, TOUCHPAD
};

class PointerAccelerationTest : public XITServerInputTest,
                                public DeviceInterface,
                                public ::testing::WithParamInterface<enum device_type>
{
public:
    virtual void SetUpMouse() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");

        driver = "evdev";
        config_section = "Option \"CorePointer\" \"on\"\n"
                         "Option \"GrabDevice\" \"on\"\n"
                         "Option \"Device\" \"" + dev->GetDeviceNode() + "\"";
    }

    virtual void SetUpTablet() {
        SetDevice("tablets/Wacom-Intuos4-6x9.desc");
        driver = "wacom";
        config_section =  "Option \"device\" \"" + dev->GetDeviceNode() + "\"\n"
                          "Option \"GrabDevice\" \"on\"\n"
                          "Option \"Type\" \"stylus\"\n"
                          "Option \"CorePointer\" \"on\"\n"
                          "Option \"Mode\" \"relative\"\n";
    }

    virtual void SetUpTouchpad() {
        SetDevice("touchpads/SynPS2-Synaptics-TouchPad.desc");
        driver = "synaptics";
        config_section =  "Option \"device\" \"" + dev->GetDeviceNode() + "\"\n"
                          "Option \"GrabDevice\" \"on\"\n"
                          "Option \"CorePointer\" \"on\"\n";
    }

    virtual void SetUp() {

        switch(GetParam()) {
            case MOUSE: SetUpMouse(); break;
            case TABLET: SetUpTablet(); break;
            case TOUCHPAD: SetUpTouchpad(); break;
        }

        XITServerInputTest::SetUp();
    }

    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.SetAutoAddDevices(false);
        config.AddInputSection(driver, "device", config_section);
        config.WriteConfig();
    }

    virtual void InitPosition(enum device_type type) {
        switch(type) {
            case MOUSE: break;
            case TABLET:
                    dev->PlayOne(EV_ABS, ABS_X, 1000);
                    dev->PlayOne(EV_ABS, ABS_Y, 1000);
                    dev->PlayOne(EV_ABS, ABS_DISTANCE, 0);
                    dev->PlayOne(EV_KEY, BTN_TOOL_PEN, 1, true);
                    break;
            case TOUCHPAD:
                    dev->PlayOne(EV_ABS, ABS_X, 1500);
                    dev->PlayOne(EV_ABS, ABS_Y, 2000);
                    dev->PlayOne(EV_ABS, ABS_PRESSURE, 34);
                    dev->PlayOne(EV_KEY, BTN_TOOL_FINGER, 1, true);
                    break;
        }
    }

    virtual void Move(enum device_type type) {
        int x, y;
        int minx, maxx, miny, maxy;

        switch(type) {
            case MOUSE:
                for (int i = 0; i < 20; i++)
                    dev->PlayOne(EV_REL, REL_X, 1, true);

                for (int i = 0; i < 20; i++)
                    dev->PlayOne(EV_REL, REL_Y, 1, true);
                break;
            case TABLET:
                dev->GetAbsData(ABS_X, &minx, &maxx);
                dev->GetAbsData(ABS_Y, &miny, &maxy);
                x = 1000;
                y = 1000;

                while (x < 10000)
                {
                    x += (maxx - minx)/10.0;
                    y += (maxy - miny)/10.0;
                    dev->PlayOne(EV_ABS, ABS_X, x);
                    dev->PlayOne(EV_ABS, ABS_Y, y);

                    dev->PlayOne(EV_ABS, ABS_DISTANCE, 0);
                    dev->PlayOne(EV_KEY, BTN_TOOL_PEN, 1, true);
                }

                dev->PlayOne(EV_KEY, BTN_TOOL_PEN, 0, true);
                break;
            case TOUCHPAD:
                dev->GetAbsData(ABS_X, &minx, &maxx);
                dev->GetAbsData(ABS_Y, &miny, &maxy);
                x = 1500;
                y = 2000;

                while (x < 3000)
                {
                    x += (maxx - minx)/10.0;
                    y += (maxy - miny)/10.0;
                    dev->PlayOne(EV_ABS, ABS_X, x);
                    dev->PlayOne(EV_ABS, ABS_Y, y);

                    dev->PlayOne(EV_ABS, ABS_PRESSURE, 40);
                    dev->PlayOne(EV_ABS, ABS_TOOL_WIDTH, 10);
                    dev->PlayOne(EV_KEY, BTN_TOUCH, 1, true);
                    dev->PlayOne(EV_KEY, BTN_TOOL_FINGER, 1, true);
                }

                dev->PlayOne(EV_KEY, BTN_TOOL_FINGER, 0, true);
                break;
        }
    }

private:
    std::string config_section;
    std::string driver;
};

TEST_P(PointerAccelerationTest, IdenticalMovementVerticalHorizontal)
{
    enum device_type device_type = GetParam();
    std::string devtype;
    switch(device_type) {
        case MOUSE: devtype = "mouse"; break;
        case TABLET: devtype = "tablet"; break;
        case TOUCHPAD: devtype = "touchpad"; break;

    }

    XORG_TESTCASE("Start server with devices in mode relative\n"
                  "Move the pointer diagonally down\n"
                  "Verify x and y relative movement are the same\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=31636");
    SCOPED_TRACE("Device type is " + devtype);

    ::Display *dpy = Display();

    ASSERT_EQ(FindInputDeviceByName(dpy, "device"), 1);

    InitPosition(device_type);

    double x1, y1;
    QueryPointerPosition(dpy, &x1, &y1);

    Move(device_type);

    double x2, y2;
    QueryPointerPosition(dpy, &x2, &y2);

    /* have we moved at all? */
    ASSERT_NE(x1, x2);
    ASSERT_NE(y1, y2);

    /* we moved diagonally, expect same accel in both directions */
    ASSERT_EQ(x2 - x1, y2 - y1);
}

INSTANTIATE_TEST_CASE_P(, PointerAccelerationTest, ::testing::Values(MOUSE, TABLET, TOUCHPAD));

class PointerButtonMotionMaskTest : public PointerMotionTest,
                                    public ::testing::WithParamInterface<int>
{
};

TEST_P(PointerButtonMotionMaskTest, ButtonXMotionMask)
{
    XORG_TESTCASE("Select for ButtonXMotionMask\n"
                  "Move pointer\n"
                  "Verify no events received\n"
                  "Press button and move\n"
                  "Verify events received\n");
    int button = GetParam();

    ::Display *dpy = Display();
    XSelectInput(dpy, DefaultRootWindow(dpy), Button1MotionMask << (button - 1));
    XSync(dpy, False);

    dev->PlayOne(EV_REL, REL_X, 10, true);
    XSync(dpy, False);
    ASSERT_EQ(XPending(dpy), 0);

    int devbutton;
    switch(button) {
        case 1: devbutton = BTN_LEFT; break;
        case 2: devbutton = BTN_MIDDLE; break;
        case 3: devbutton = BTN_RIGHT; break;
        default: FAIL();
    }

    dev->PlayOne(EV_KEY, devbutton, 1, true);
    dev->PlayOne(EV_REL, REL_X, 10, true);
    dev->PlayOne(EV_KEY, devbutton, 0, true);

    ASSERT_EVENT(XEvent, motion, dpy, MotionNotify);
    ASSERT_TRUE(motion->xmotion.state & (Button1Mask << (button -1)));

    XSync(dpy, False);
    ASSERT_EQ(XPending(dpy), 0);
}

INSTANTIATE_TEST_CASE_P(, PointerButtonMotionMaskTest, ::testing::Range(1, 4));

class PointerSubpixelTest : public PointerMotionTest {
    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n"
                               "Option \"ConstantDeceleration\" \"20\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "kbd-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }
};

TEST_F(PointerSubpixelTest, NoSubpixelCoreEvents)
{
    XORG_TESTCASE("Move pointer by less than a pixels\n"
                  "Ensure no core motion event is received\n"
                  "Ensure XI2 motion events are received\n");

    ::Display *dpy = Display();
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy2);

    double x, y;
    QueryPointerPosition(dpy, &x, &y);

    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask);
    XSync(dpy, False);

    dev->PlayOne(EV_REL, REL_X, 1, true);
    dev->PlayOne(EV_REL, REL_X, 1, true);
    dev->PlayOne(EV_REL, REL_X, 1, true);
    dev->PlayOne(EV_REL, REL_X, 1, true);

    ASSERT_FALSE(XServer::WaitForEvent(Display(), 500));

    XSelectInput(dpy, DefaultRootWindow(dpy), NoEventMask);
    XSync(dpy, False);

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_Motion);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_Motion);

    XISelectEvents(dpy2, DefaultRootWindow(dpy), &mask, 1);
    XSync(dpy2, False);

    delete[] mask.mask;

    dev->PlayOne(EV_REL, REL_X, 1, true);
    dev->PlayOne(EV_REL, REL_X, 1, true);
    dev->PlayOne(EV_REL, REL_X, 1, true);
    dev->PlayOne(EV_REL, REL_X, 1, true);

    ASSERT_TRUE(XServer::WaitForEventOfType(dpy2, GenericEvent, xi2_opcode, XI_Motion));

    double new_x, new_y;
    QueryPointerPosition(dpy, &new_x, &new_y);
    ASSERT_EQ(x, new_x);
    ASSERT_EQ(y, new_y);

    XCloseDisplay(dpy2);
}

class PointerRelativeTransformationMatrixTest : public PointerMotionTest {
public:
    void SetDeviceMatrix(::Display *dpy, int deviceid, struct pixman_f_transform *m) {
        float matrix[9];

        for (int i = 0; i < 9; i++)
            matrix[i] = m->m[i/3][i%3];

        Atom prop = XInternAtom(dpy, "Coordinate Transformation Matrix", True);
        XIChangeProperty(dpy, deviceid, prop, XInternAtom(dpy, "FLOAT", True),
                         32, PropModeReplace,
                         reinterpret_cast<unsigned char*>(matrix), 9);
    }

    void DisablePtrAccel(::Display *dpy, int deviceid) {
        int data = -1;

        Atom prop = XInternAtom(dpy, "Device Accel Profile", True);
        XIChangeProperty(dpy, deviceid, prop, XA_INTEGER, 32,
                         PropModeReplace, reinterpret_cast<unsigned char*>(&data), 1);
    }

    void MoveAndCompare(::Display *dpy, int dx, int dy) {
        double x, y;

        QueryPointerPosition(dpy, &x, &y);
        dev->PlayOne(EV_REL, REL_X, dx);
        dev->PlayOne(EV_REL, REL_Y, dy, true);
        ASSERT_EVENT(XIDeviceEvent, motion, dpy, GenericEvent, xi2_opcode, XI_Motion);
        ASSERT_EQ(motion->root_x, x + dx);
        ASSERT_EQ(motion->root_y, y + dy);
    }
};


TEST_F(PointerRelativeTransformationMatrixTest, IgnoreTranslationComponent)
{
    XORG_TESTCASE("Apply a translation matrix to the device\n"
                  "Move the pointer.\n"
                  "Verify matrix does not affect movement\n");

    ::Display *dpy = Display();

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--", &deviceid), 1);

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_Motion);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_Motion);

    XISelectEvents(dpy, DefaultRootWindow(dpy), &mask, 1);

    DisablePtrAccel(dpy, deviceid);

    struct pixman_f_transform m;
    pixman_f_transform_init_translate(&m, 10, 0);
    SetDeviceMatrix(dpy, deviceid, &m);

    MoveAndCompare(dpy, 10, 0);
    MoveAndCompare(dpy, 0, 10);
    MoveAndCompare(dpy, 5, 5);

    pixman_f_transform_init_translate(&m, 0, 10);
    SetDeviceMatrix(dpy, deviceid, &m);

    MoveAndCompare(dpy, 10, 0);
    MoveAndCompare(dpy, 0, 10);
    MoveAndCompare(dpy, 5, 5);
}

class PointerRelativeRotationMatrixTest : public PointerRelativeTransformationMatrixTest,
                                          public ::testing::WithParamInterface<int> {
};

TEST_P(PointerRelativeRotationMatrixTest, RotationTest)
{
    XORG_TESTCASE("Apply a coordinate transformation to the relative device\n"
                  "Move the pointer.\n"
                  "Verify movement against matrix\n");

    ::Display *dpy = Display();

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--", &deviceid), 1);

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_Motion);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_Motion);

    XISelectEvents(dpy, DefaultRootWindow(dpy), &mask, 1);

    int angle = GetParam() * M_PI/180.0;

    struct pixman_f_transform m;
    pixman_f_transform_init_rotate(&m, cos(angle), sin(angle));

    SetDeviceMatrix(dpy, deviceid, &m);

    int coords[][2] = {
        {1, 0}, {-2, 0}, {3, 2}, {4, -7},
        {-3, 6}, {-5, -9}, {0, 3}, {0, -5},
        {0, 0}, /* null-terminated */
    };

    int i = 0;
    while(coords[i][0] && coords[i][1]) {
        double dx = coords[i][0], dy = coords[i][1];
        struct pixman_f_vector p = { .v = {dx, dy, 1} };

        ASSERT_TRUE(pixman_f_transform_point(&m, &p));

        double x, y;
        QueryPointerPosition(dpy, &x, &y);

        /* Move pointer */
        dev->PlayOne(EV_REL, REL_X, dx);
        dev->PlayOne(EV_REL, REL_Y, dy, true);

        /* Compare to two decimal places */
        ASSERT_EVENT(XIDeviceEvent, motion, dpy, GenericEvent, xi2_opcode, XI_Motion);
        ASSERT_LT(fabs(motion->root_x - (x + p.v[0])), 0.001);
        ASSERT_LT(fabs(motion->root_y - (y + p.v[1])), 0.001);
        i++;
    }

}

INSTANTIATE_TEST_CASE_P(, PointerRelativeRotationMatrixTest, ::testing::Range(0, 360, 15));
