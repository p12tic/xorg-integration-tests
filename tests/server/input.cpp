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
