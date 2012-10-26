#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdexcept>
#include <map>
#include <unistd.h>

#include <xorg/gtest/xorg-gtest.h>
#include <linux/input.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#define XK_LATIN1
#include <X11/keysymdef.h>
#include <X11/XF86keysym.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "wacom_devs.h"

#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include <xorg/wacom-properties.h>
#include <unistd.h>

#include "input-driver-test.h"
#include "device-interface.h"
#include "helpers.h"

/**
 * Wacom driver test class. This class takes a struct Tablet that defines
 * which device should be initialised.
 */
class WacomDriverTest : public InputDriverTest,
                        public DeviceInterface,
                        public ::testing::WithParamInterface<Tablet> {
protected:

    /**
     * Write a configuration file for a hotplug-enabled server using the
     * dummy video driver.
     */
    virtual void SetUpConfigAndLog() {
        InitDefaultLogFiles(server, &config);

        config.AddDefaultScreenWithDriver();
        config.SetAutoAddDevices(true);
        config.WriteConfig();
    }

    /**
     * Register for XI2 Hierarchy events on the root window
     */
    void SetUpXIEventMask () {
        XIEventMask evmask;
        unsigned char mask[2] = { 0, 0 };

        XISetMask(mask, XI_HierarchyChanged);

        evmask.deviceid = XIAllDevices;
        evmask.mask_len = sizeof(mask);
        evmask.mask = mask;

        EXPECT_EQ(Success, XISelectEvents(Display(), DefaultRootWindow(Display()), &evmask, 1));
        XSync(Display(), False);
    }

    /**
     * Create an evemu device based on GetParam()'s tablet.
     */
    void VerifyDevice (const Tablet& tablet)
    {
        char tool_name[255];

        if (tablet.stylus) {
            snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.stylus);
            if (!FindInputDeviceByName(Display(), tool_name)) {
                ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), tool_name, 5000))
                    << "Tool '" << tool_name << "' not found" << std::endl;
            }
        }

        if (tablet.eraser) {
            snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.eraser);
            if (!FindInputDeviceByName(Display(), tool_name)) {
                ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), tool_name, 5000))
                    << "Tool '" << tool_name << "' not found" << std::endl;
            }
        }

        if (tablet.cursor) {
            snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.cursor);
            if (!FindInputDeviceByName(Display(), tool_name)) {
                ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), tool_name, 5000))
                    << "Tool '" << tool_name << "' not found" << std::endl;
            }
        }

        if (tablet.touch) {
            snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.touch);
            if (!FindInputDeviceByName(Display(), tool_name)) {
                ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), tool_name, 5000))
                    << "Tool '" << tool_name << "' not found" << std::endl;
            }
        }

        if (tablet.pad) {
            snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.pad);
            if (!FindInputDeviceByName(Display(), tool_name)) {
                ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), tool_name, 5000))
                    << "Tool '" << tool_name << "' not found" << std::endl;
            }
        }
    }

    virtual void SetUp()
    {
        Tablet tablet = GetParam();
        SetDevice("tablets/" + std::string (tablet.descfile));

        InputDriverTest::SetUp();

        SetUpXIEventMask ();
        VerifyDevice(tablet);
    }
};

/* Return True if the given device has the property, or False otherwise */
bool test_property(Display *dpy, int deviceid, const char *prop_name)
{
    Status status;
    Atom prop;
    Atom realtype;
    int realformat;
    unsigned long nitems, bytes_after;
    unsigned char *data;

    prop = XInternAtom (dpy, prop_name, False);
    status = XIGetProperty(dpy, deviceid, prop , 0, 1000, False,
                           AnyPropertyType, &realtype, &realformat,
                           &nitems, &bytes_after, &data);
    if (data != NULL)
        XFree (data);

    if (status == Success)
        return True;

    return False;
}

TEST_P(WacomDriverTest, DeviceNames)
{
    XORG_TESTCASE("Test names of the tools and type properties match the tools");

    Tablet tablet = GetParam();

    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));

    bool prop_found;

    /* Wait for devices to settle */

    int deviceid;
    char tool_name[255];

    if (tablet.stylus) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.stylus);
        ASSERT_EQ(FindInputDeviceByName(Display(), tool_name, &deviceid), 1) <<  "Tool not found " << tool_name <<std::endl;

        prop_found = test_property (Display(), deviceid, WACOM_PROP_PRESSURECURVE);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_PRESSURECURVE << " not found on " << tool_name << std::endl;

        prop_found = test_property (Display(), deviceid, WACOM_PROP_ROTATION);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_ROTATION << " not found on " << tool_name << std::endl;

        prop_found = test_property (Display(), deviceid, WACOM_PROP_TOOL_TYPE);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_TOOL_TYPE << " not found on " << tool_name << std::endl;
    }

    if (tablet.eraser) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.eraser);
        ASSERT_EQ(FindInputDeviceByName(Display(), tool_name, &deviceid), 1) <<  "Tool not found " << tool_name <<std::endl;

        prop_found = test_property (Display(), deviceid, WACOM_PROP_PRESSURECURVE);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_PRESSURECURVE << " not found on " << tool_name << std::endl;

        prop_found = test_property (Display(), deviceid, WACOM_PROP_ROTATION);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_ROTATION << " not found on " << tool_name << std::endl;
    }

    if (tablet.cursor) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.cursor);
        ASSERT_EQ(FindInputDeviceByName(Display(), tool_name, &deviceid), 1) <<  "Tool not found " << tool_name <<std::endl;

        prop_found = test_property (Display(), deviceid,  WACOM_PROP_WHEELBUTTONS);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_WHEELBUTTONS << " not found on " << tool_name << std::endl;
    }

    if (tablet.pad) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.pad);
        ASSERT_EQ(FindInputDeviceByName(Display(), tool_name, &deviceid), 1) <<  "Tool not found " << tool_name <<std::endl;

        prop_found = test_property (Display(), deviceid,  WACOM_PROP_STRIPBUTTONS);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_STRIPBUTTONS << " not found on " << tool_name << std::endl;
    }

    if (tablet.touch) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.touch);
        ASSERT_EQ(FindInputDeviceByName(Display(), tool_name, &deviceid), 1) <<  "Tool not found " << tool_name <<std::endl;

        prop_found = test_property (Display(), deviceid,  WACOM_PROP_TOUCH);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_TOUCH << " not found on " << tool_name << std::endl;

        prop_found = test_property (Display(), deviceid,  WACOM_PROP_ENABLE_GESTURE);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_ENABLE_GESTURE << " not found on " << tool_name << std::endl;

        prop_found = test_property (Display(), deviceid,  WACOM_PROP_GESTURE_PARAMETERS);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_GESTURE_PARAMETERS << " not found on " << tool_name << std::endl;
    }
}

void check_for_type (Display *dpy, XIDeviceInfo *list, int ndevices, const char *type)
{
    XIDeviceInfo *info;
    int loop;
    bool found = false;

    info = list;
    for (loop = 0; loop < ndevices; loop++, info++) {
        if (test_property (dpy, info->deviceid, type)) {
                found = True;
                break;
        }
    }
    ASSERT_TRUE(found) << type << " not found" << std::endl;
}

TEST_P(WacomDriverTest, DeviceType)
{
    XORG_TESTCASE("Test type of the tools for the device");

    Tablet tablet = GetParam();
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));

    XIDeviceInfo *list;
    int ndevices;
    list = XIQueryDevice(Display(), XIAllDevices, &ndevices);

    if (tablet.stylus)
        check_for_type (Display(), list, ndevices, WACOM_PROP_XI_TYPE_STYLUS);

    if (tablet.eraser)
        check_for_type (Display(), list, ndevices, WACOM_PROP_XI_TYPE_ERASER);

    if (tablet.cursor)
        check_for_type (Display(), list, ndevices, WACOM_PROP_XI_TYPE_CURSOR);

    if (tablet.pad)
        check_for_type (Display(), list, ndevices, WACOM_PROP_XI_TYPE_PAD);

    if (tablet.touch)
        check_for_type (Display(), list, ndevices, WACOM_PROP_XI_TYPE_TOUCH);

    XIFreeDeviceInfo (list);
}

bool set_rotate(Display *dpy, int deviceid, const char *rotate)
{
    Status status;
    int rotation = 0;
    Atom prop, type;
    int format;
    unsigned char* data;
    unsigned long nitems, bytes_after;

    if (strcasecmp(rotate, "cw") == 0)
        rotation = 1;
    else if (strcasecmp(rotate, "ccw") == 0)
        rotation = 2;
    else if (strcasecmp(rotate, "half") == 0)
        rotation = 3;

    prop = XInternAtom(dpy, WACOM_PROP_ROTATION, True);
    if (!prop)
        return False;

    status = XIGetProperty(dpy, deviceid, prop, 0, 1000, False,
                           AnyPropertyType, &type, &format,
                           &nitems, &bytes_after, &data);

    if (status != Success || nitems == 0 || format != 8)
        return False;

    *data = rotation;

    XIChangeProperty(dpy, deviceid, prop, type, format,
                          PropModeReplace, data, nitems);
    XFlush(dpy);

    return True;
}

int stylus_move_right (Display *dpy, xorg::testing::evemu::Device *dev)
{
    int root_x1, root_x2;
    int loop, step;
    XEvent ev;

    XSync(dpy, False);
    while(XPending(dpy))
        XNextEvent(dpy, &ev);

    // Might be considered as uninitialized otherwise
    root_x1 = 0;

    // Move to device coord (1000,1000)
    dev->PlayOne(EV_ABS, ABS_X, 1000, True);
    dev->PlayOne(EV_ABS, ABS_Y, 1000, True);
    dev->PlayOne(EV_ABS, ABS_DISTANCE, 0, True);
    dev->PlayOne(EV_KEY, BTN_TOOL_PEN, 1, True);

    XSync (dpy, False);
    EXPECT_NE(XPending(dpy), 0) << "No event received??" << std::endl;

    // Capture motion events and save screen coordinates
    XSync (dpy, False);
    while(XCheckMaskEvent (dpy, PointerMotionMask, &ev))
        root_x1 = ev.xmotion.x_root;

    root_x2 = root_x1;

    step = 10;
    for (loop = 1000; loop < 3000; loop += step) {
        dev->PlayOne(EV_ABS, ABS_X, loop, True);
        dev->PlayOne(EV_ABS, ABS_DISTANCE, step, True);
    }
    dev->PlayOne(EV_KEY, BTN_TOOL_PEN, 0, True);

    XSync (dpy, False);
    EXPECT_NE(XPending(dpy), 0) << "Still no event received??" << std::endl;

    XSync (dpy, False);
    while(XCheckMaskEvent (dpy, PointerMotionMask|PointerMotionHintMask, &ev))
        root_x2 = ev.xmotion.x_root;

    XSync (dpy, False);
    while(XPending(dpy))
        XNextEvent(dpy, &ev);

    return (root_x2 - root_x1);
}

// FIXME
// This test would fail in some random case, as it the X events were
// not generated...
TEST_P(WacomDriverTest, Rotation)
{
    XORG_TESTCASE("Test rotation is applied on a device");

    Tablet tablet = GetParam();

    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));

    bool status;

    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask |
                                                          ButtonReleaseMask |
                                                          PointerMotionMask |
                                                          ButtonMotionMask);

    /* the server takes a while to start up but the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());

    char tool_name[255];
    int displace;
    int deviceid;

    if (tablet.stylus) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.stylus);
        ASSERT_EQ(FindInputDeviceByName(Display(), tool_name, &deviceid), 1) <<  "Tool not found " << tool_name <<std::endl;

        // Try with no rotation
        status = set_rotate (Display(), deviceid, "none");
        EXPECT_TRUE(status) << "Failed to rotate " << tool_name << std::endl;

        displace = stylus_move_right (Display(), dev.get());
        ASSERT_NE (displace, 0) << "Did not get any event from " << tool_name << std::endl;
        EXPECT_TRUE(displace > 0) << "Pointer moved to the wrong direction with rotate none for " << tool_name << std::endl;

        // Set opposite rotation
        status = set_rotate (Display(), deviceid, "half");
        EXPECT_TRUE(status) << "Failed to rotate " << tool_name << std::endl;

        displace = stylus_move_right (Display(), dev.get());
        ASSERT_NE (displace, 0) << "Did not get any event from " << tool_name << std::endl;
        EXPECT_TRUE(displace < 0) << "Pointer moved to the wrong direction with rotate half for " << tool_name << std::endl;
    }
}

INSTANTIATE_TEST_CASE_P(, WacomDriverTest,
        ::testing::ValuesIn(tablets));

TEST(WacomDriver, PrivToolDoubleFree)
{
    XORG_TESTCASE("Two devices with the same device node cause a double-free\n"
                 "of priv->tool in wcmPreInitParseOptions.\n"
                 "https://bugs.freedesktop.org/show_bug.cgi?id=55200");

    XOrgConfig config;
    XITServer server;

    std::auto_ptr<xorg::testing::evemu::Device> stylus =
        std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(RECORDINGS_DIR "tablets/Wacom-Intuos5-touch-M-Pen.desc")
                );

    config.AddInputSection("wacom", "I5 stylus",
                           "Option \"Device\" \"" + stylus->GetDeviceNode() + "\"\n"
                           "Option \"USB\" \"on\"\n"
                           "Option \"Type\" \"stylus\"\n"
                           );

    config.AddInputSection("wacom", "I5 stylus 2",
                           "Option \"Device\" \"" + stylus->GetDeviceNode() + "\"\n"
                           "Option \"USB\" \"on\"\n"
                           "Option \"Type\" \"stylus\"\n"
                           );
    config.AddDefaultScreenWithDriver();

    EXPECT_NO_THROW(StartServer("wacom-double-free", server, config));
}

class WacomLensCursorTest : public WacomDriverTest
{
};

TEST_P(WacomLensCursorTest, CursorMove)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask |
                                                          ButtonReleaseMask |
                                                          PointerMotionMask |
                                                          ButtonMotionMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "tablets/Wacom-Intuos4-8x13.lens-cursor-move.events");

    XSync(Display(), False);

    ASSERT_TRUE(XPending(Display()));

    XEvent ev;
    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.xany.type, MotionNotify);
    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.xany.type, MotionNotify);
    XNextEvent(Display(), &ev);
    ASSERT_EQ(ev.xany.type, MotionNotify);
}

INSTANTIATE_TEST_CASE_P(, WacomLensCursorTest, ::testing::ValuesIn(lens_cursor_tablets));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
