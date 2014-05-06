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
#include <stdint.h>
#include <map>
#include <unistd.h>

#include <xorg/gtest/xorg-gtest.h>
#include <linux/input.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
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

#include "xit-server-input-test.h"
#include "xit-property.h"
#include "xit-event.h"
#include "device-interface.h"
#include "helpers.h"

/**
 * Wacom driver test class for one device (Intuos4). Use this to test driver
 * features that don't depend on the actual device much.
 */
class WacomDeviceTest : public XITServerInputTest,
                        public DeviceInterface {
public:

    /**
     * @return button number for idx minus the 4 scroll buttons
     */
    virtual int ButtonNumber(int idx) const {
        return ((idx > 3) ? idx - 4 : idx);
    }

    virtual void TipDown(void) {
        dev->PlayOne(EV_ABS, ABS_X, 4000);
        dev->PlayOne(EV_ABS, ABS_Y, 4000);
        dev->PlayOne(EV_KEY, BTN_TOOL_PEN, 1);
        dev->PlayOne(EV_SYN, SYN_REPORT, 0);

        dev->PlayOne(EV_ABS, ABS_X, 4200);
        dev->PlayOne(EV_ABS, ABS_Y, 4200);
        dev->PlayOne(EV_ABS, ABS_DISTANCE, 0);
        dev->PlayOne(EV_ABS, ABS_PRESSURE, 70);
        dev->PlayOne(EV_KEY, BTN_TOUCH, 1);
        dev->PlayOne(EV_SYN, SYN_REPORT, 0);
    }

    virtual void TipUp(void) {
        dev->PlayOne(EV_ABS, ABS_X, 4000);
        dev->PlayOne(EV_ABS, ABS_Y, 4000);
        dev->PlayOne(EV_ABS, ABS_DISTANCE, 0);
        dev->PlayOne(EV_ABS, ABS_PRESSURE, 0);
        dev->PlayOne(EV_KEY, BTN_TOOL_PEN, 0);
        dev->PlayOne(EV_KEY, BTN_TOUCH, 0);
        dev->PlayOne(EV_SYN, SYN_REPORT, 0);
    }

    virtual void SetUp()
    {
        SetDevice("tablets/Wacom-Intuos4-6x9.desc");

        XITServerInputTest::SetUp();
    }

    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputClass("wacom", "MatchDriver \"wacom\"",
                             "Option \"GrabDevice\" \"on\""
                             "Option \"CommonDBG\" \"12\""
                             "Option \"DebugLevel\" \"12\"");
        config.SetAutoAddDevices(true);
        config.WriteConfig();
    }
};


/**
 * Wacom driver test class. This class takes a struct Tablet that defines
 * which device should be initialised.
 */
class WacomDriverTest : public WacomDeviceTest,
                        public ::testing::WithParamInterface<Tablet> {
protected:
    /**
     * Write a configuration file for a hotplug-enabled server using the
     * dummy video driver.
     */
    virtual void SetUpConfigAndLog() {
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

        XITServerInputTest::SetUp();

        SetUpXIEventMask ();
        VerifyDevice(tablet);
    }
};

TEST_P(WacomDriverTest, DeviceNames)
{
    XORG_TESTCASE("Test names of the tools and type properties match the tools");

    Tablet tablet = GetParam();

    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));


    int deviceid;
    char tool_name[255];

    if (tablet.stylus) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.stylus);
        ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), tool_name, 5000));
        ASSERT_EQ(FindInputDeviceByName(Display(), tool_name, &deviceid), 1) <<  "Tool not found " << tool_name <<std::endl;

        EXPECT_TRUE(DevicePropertyExists(Display(), deviceid, WACOM_PROP_PRESSURECURVE))
                    << "Property " << WACOM_PROP_PRESSURECURVE << " not found on " << tool_name << std::endl;
        EXPECT_TRUE(DevicePropertyExists(Display(), deviceid, WACOM_PROP_ROTATION))
                    << "Property " << WACOM_PROP_ROTATION << " not found on " << tool_name << std::endl;
        EXPECT_TRUE(DevicePropertyExists(Display(), deviceid, WACOM_PROP_TOOL_TYPE))
                    << "Property " << WACOM_PROP_TOOL_TYPE << " not found on " << tool_name << std::endl;
    }

    if (tablet.eraser) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.eraser);
        ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), tool_name, 5000));
        ASSERT_EQ(FindInputDeviceByName(Display(), tool_name, &deviceid), 1) <<  "Tool not found " << tool_name <<std::endl;

        EXPECT_TRUE(DevicePropertyExists(Display(), deviceid, WACOM_PROP_PRESSURECURVE))
                    << "Property " << WACOM_PROP_PRESSURECURVE << " not found on " << tool_name << std::endl;
        EXPECT_TRUE(DevicePropertyExists(Display(), deviceid, WACOM_PROP_ROTATION))
                    << "Property " << WACOM_PROP_ROTATION << " not found on " << tool_name << std::endl;
    }

    if (tablet.cursor) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.cursor);
        ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), tool_name, 5000));
        ASSERT_EQ(FindInputDeviceByName(Display(), tool_name, &deviceid), 1) <<  "Tool not found " << tool_name <<std::endl;

        EXPECT_TRUE(DevicePropertyExists(Display(), deviceid, WACOM_PROP_WHEELBUTTONS))
                    << "Property " << WACOM_PROP_WHEELBUTTONS << " not found on " << tool_name << std::endl;
    }

    if (tablet.pad) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.pad);
        ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), tool_name, 5000));
        ASSERT_EQ(FindInputDeviceByName(Display(), tool_name, &deviceid), 1) <<  "Tool not found " << tool_name <<std::endl;

        EXPECT_TRUE(DevicePropertyExists(Display(), deviceid, WACOM_PROP_STRIPBUTTONS))
                    << "Property " << WACOM_PROP_STRIPBUTTONS << " not found on " << tool_name << std::endl;
    }

    if (tablet.touch) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.touch);
        ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), tool_name, 5000));
        ASSERT_EQ(FindInputDeviceByName(Display(), tool_name, &deviceid), 1) <<  "Tool not found " << tool_name <<std::endl;

        EXPECT_TRUE(DevicePropertyExists(Display(), deviceid, WACOM_PROP_TOUCH))
                    << "Property " << WACOM_PROP_TOUCH << " not found on " << tool_name << std::endl;
        EXPECT_TRUE(DevicePropertyExists(Display(), deviceid, WACOM_PROP_ENABLE_GESTURE))
                    << "Property " << WACOM_PROP_ENABLE_GESTURE << " not found on " << tool_name << std::endl;
        EXPECT_TRUE(DevicePropertyExists(Display(), deviceid, WACOM_PROP_GESTURE_PARAMETERS))
                    << "Property " << WACOM_PROP_GESTURE_PARAMETERS << " not found on " << tool_name << std::endl;
    }
}

void check_for_type (Display *dpy, XIDeviceInfo *list, int ndevices, const char *type)
{
    XIDeviceInfo *info;
    int loop;
    bool found = false;
    Atom tool_type = XInternAtom(dpy, type, True);

    ASSERT_NE((Atom)None, tool_type);

    info = list;
    for (loop = 0; loop < ndevices; loop++, info++) {
        if (DevicePropertyExists(dpy, info->deviceid, WACOM_PROP_TOOL_TYPE)) {
            ASSERT_PROPERTY(Atom, prop, dpy, info->deviceid, WACOM_PROP_TOOL_TYPE);
            if (prop.data[0] == tool_type) {
                found = True;
                break;
            }
        }
    }
    ASSERT_TRUE(found) << type << " not found" << std::endl;
}

TEST_P(WacomDriverTest, DeviceType)
{
    XORG_TESTCASE("Test type of the tools for the device");

    Tablet tablet = GetParam();

    VerifyDevice(tablet);

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

void set_rotate(Display *dpy, int deviceid, const char *rotate)
{
    int rotation = 0;

    if (strcasecmp(rotate, "cw") == 0)
        rotation = 1;
    else if (strcasecmp(rotate, "ccw") == 0)
        rotation = 2;
    else if (strcasecmp(rotate, "half") == 0)
        rotation = 3;

    ASSERT_PROPERTY(char, prop, dpy, deviceid, WACOM_PROP_ROTATION);

    prop.data[0] = rotation;
    prop.Update();
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
    dev->PlayOne(EV_ABS, ABS_X, 1000);
    dev->PlayOne(EV_ABS, ABS_Y, 1000);
    dev->PlayOne(EV_ABS, ABS_DISTANCE, 0);
    dev->PlayOne(EV_KEY, BTN_TOOL_PEN, 1, true);

    XSync (dpy, False);
    EXPECT_NE(XPending(dpy), 0) << "No event received??" << std::endl;

    // Capture motion events and save screen coordinates
    XSync (dpy, False);
    while(XCheckMaskEvent (dpy, PointerMotionMask, &ev))
        root_x1 = ev.xmotion.x_root;

    root_x2 = root_x1;

    step = 10;
    for (loop = 1000; loop < 3000; loop += step) {
        dev->PlayOne(EV_ABS, ABS_X, loop);
        dev->PlayOne(EV_ABS, ABS_DISTANCE, step, true);
    }
    dev->PlayOne(EV_KEY, BTN_TOOL_PEN, 0, true);

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

    VerifyDevice(tablet);
    XSync(Display(), True);

    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask |
                                                          ButtonReleaseMask |
                                                          PointerMotionMask |
                                                          ButtonMotionMask);

    char tool_name[255];
    int displace;
    int deviceid;

    if (tablet.stylus) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.stylus);
        ASSERT_EQ(FindInputDeviceByName(Display(), tool_name, &deviceid), 1) <<  "Tool not found " << tool_name <<std::endl;

        // Try with no rotation
        set_rotate (Display(), deviceid, "none");

        displace = stylus_move_right (Display(), dev.get());
        ASSERT_NE (displace, 0) << "Did not get any event from " << tool_name << std::endl;
        EXPECT_TRUE(displace > 0) << "Pointer moved to the wrong direction with rotate none for " << tool_name << std::endl;

        // Set opposite rotation
        set_rotate (Display(), deviceid, "half");

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

    EXPECT_NO_THROW(server.Start(config));
}

class WacomLensCursorTest : public WacomDriverTest
{
};

TEST_P(WacomLensCursorTest, CursorMove)
{
    ::Display *dpy = Display();

    XSelectInput(dpy, DefaultRootWindow(dpy), ButtonPressMask |
                                              ButtonReleaseMask |
                                              PointerMotionMask |
                                              ButtonMotionMask);
    XSync(dpy, False);

    dev->Play(RECORDINGS_DIR "tablets/Wacom-Intuos4-8x13.lens-cursor-move.events");

    XSync(dpy, False);

    ASSERT_EVENT(XEvent, m1, dpy, MotionNotify);
    ASSERT_EVENT(XEvent, m2, dpy, MotionNotify);
    ASSERT_EVENT(XEvent, m3, dpy, MotionNotify);
}

INSTANTIATE_TEST_CASE_P(, WacomLensCursorTest, ::testing::ValuesIn(lens_cursor_tablets));

class WacomToolProximityTest : public XITServerInputTest,
                               public DeviceInterface,
                               public ::testing::WithParamInterface<int> {
public:
    virtual void SetUpConfigAndLog() {
        std::string tool_type;
        switch (GetParam()) {
            case BTN_TOOL_PEN:
            case BTN_TOOL_BRUSH:
            case BTN_TOOL_AIRBRUSH:
            case BTN_TOOL_PENCIL:
                tool_type = "stylus";
                break;
            case BTN_TOOL_RUBBER:
                tool_type = "eraser";
                break;
            case BTN_TOOL_MOUSE:
            case BTN_TOOL_LENS:
                tool_type = "cursor";
                break;
            default:
                FAIL() << "Invalid tool type";
        }

        config.AddDefaultScreenWithDriver();
        config.SetAutoAddDevices(false);
        config.AddInputSection("wacom", "--cursor--",
                               "Option \"Type\" \"" + tool_type + "\"\n"
                               "Option \"GrabDevice\" \"off\"\n"
                               "Option \"DebugLevel\" \"12\"\n"
                               "Option \"CommonDBG\" \"12\"\n"
                               "Option \"CursorProx\" \"63\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        config.WriteConfig();
    }

    virtual void SetUp() {
        SetDevice("tablets/Wacom-Intuos4-8x13.desc");

        int tool_type = GetParam();

        /* bring device into prox */
        dev->PlayOne(EV_ABS, ABS_X, 6784, false);
        dev->PlayOne(EV_ABS, ABS_Y, 2946, false);
        dev->PlayOne(EV_ABS, ABS_DISTANCE, 63, false);
        dev->PlayOne(EV_KEY, tool_type, 1, false);
        dev->PlayOne(EV_MSC, MSC_SERIAL, -494927850, true);

        dev->PlayOne(EV_ABS, ABS_X, 6740, false);
        dev->PlayOne(EV_ABS, ABS_DISTANCE, 45, false);
        dev->PlayOne(EV_MSC, MSC_SERIAL, -494927850, true);

        XITServerInputTest::SetUp();
    }
};


TEST_P(WacomToolProximityTest, ToolMovesOnProximity)
{
    XORG_TESTCASE("Create device with the given tool.\n"
                  "Get tool into proximity.\n"
                  "Start server.\n"
                  "Check that events from the tool move the cursor\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=56536");

    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask |
                                                          ButtonReleaseMask |
                                                          PointerMotionMask |
                                                          ButtonMotionMask);
    dev->PlayOne(EV_ABS, ABS_X, 6800, false);
    dev->PlayOne(EV_ABS, ABS_Y, 2900, false);
    dev->PlayOne(EV_ABS, ABS_DISTANCE, 40, false);
    dev->PlayOne(EV_MSC, MSC_SERIAL, -494927850, true);

    dev->PlayOne(EV_ABS, ABS_X, 6800, false);
    dev->PlayOne(EV_ABS, ABS_Y, 2900, false);
    dev->PlayOne(EV_ABS, ABS_DISTANCE, 40, false);
    dev->PlayOne(EV_MSC, MSC_SERIAL, -494927850, true);


    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(), MotionNotify));
}

INSTANTIATE_TEST_CASE_P(, WacomToolProximityTest, ::testing::Values(BTN_TOOL_PEN,
                                                                    BTN_TOOL_AIRBRUSH,
                                                                    BTN_TOOL_PENCIL,
                                                                    BTN_TOOL_BRUSH,
                                                                    BTN_TOOL_RUBBER,
                                                                    BTN_TOOL_LENS,
                                                                    BTN_TOOL_MOUSE));


class WacomPropertyTest : public WacomDeviceTest {
public:
    virtual void SetUp(void) {
        std::string name = "Wacom Intuos4 6x9 stylus";
        WacomDeviceTest::SetUp();
        ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), name, 5000));
        XSync(Display(), True);
        ASSERT_EQ(FindInputDeviceByName(Display(), name, &stylus_id), 1);
    }

    bool PropertyExists(::Display *dpy, int deviceid, const std::string &propname)
    {
        Atom prop;

        prop = XInternAtom(dpy, propname.c_str(), True);
        if (prop == None)
            return false;


        Atom *props;
        int nprops;
        props = XIListProperties(dpy, deviceid, &nprops);

        bool prop_found = false;
        while (nprops-- && !prop_found)
            prop_found = (props[nprops] == prop);

        XFree(props);

        return prop_found;
    }

    unsigned char *GetPropertyData(::Display *dpy, int deviceid, const std::string &propname)
    {
        Atom prop;

        prop = XInternAtom(dpy, propname.c_str(), True);
        if (prop == None)
            return NULL;

        unsigned char *data;
        Atom type;
        int format;
        unsigned long nitems, bytes_after;
        XIGetProperty(dpy, stylus_id, prop, 0, 100, False,
                      AnyPropertyType, &type, &format, &nitems, &bytes_after,
                      &data);
        return data;
    }

    int stylus_id;
};

TEST_F(WacomPropertyTest, ButtonActionPropertiesPresent)
{
    XORG_TESTCASE("Ensure the button action properties are present.\n");

    ::Display *dpy = Display();

    unsigned int nbuttons = 0;
    XIDeviceInfo *info;
    int ndevices;
    info = XIQueryDevice(dpy, stylus_id, &ndevices);
    ASSERT_EQ(ndevices, 1);
    ASSERT_TRUE(info != NULL);

    for (int i = 0; i < info->num_classes; i++) {
        if (info->classes[i]->type != XIButtonClass)
            continue;

        XIButtonClassInfo *b = reinterpret_cast<XIButtonClassInfo*>(info->classes[i]);
        nbuttons = b->num_buttons;
        break;
    }
    XIFreeDeviceInfo(info);

    ASSERT_GT(nbuttons, 0U);

    ASSERT_PROPERTY(Atom, btnaction_prop, dpy, stylus_id, "Wacom Button Actions");
    ASSERT_EQ(btnaction_prop.type, XA_ATOM);
    ASSERT_EQ(btnaction_prop.nitems, nbuttons);
    ASSERT_EQ(btnaction_prop.format, 32);

    for (unsigned int i = 0; i < nbuttons; i++) {
        if (i >= 3 && i <= 6) { /* scroll buttons, 0 indexed */
            ASSERT_EQ(btnaction_prop.data[i], (Atom)None);
            continue;
        }

        std::stringstream propname;
        propname << "Wacom button action " << ButtonNumber(i);

        ASSERT_PROPERTY(int, action_prop, dpy, stylus_id, propname.str());
        ASSERT_EQ(btnaction_prop.data[i], action_prop.prop);
    }
}

TEST_F(WacomPropertyTest, Button1DoubleMiddleClick)
{
    XORG_TESTCASE("Change Button0 action property to middle double click\n"
                  "Trigger tip event.\n"
                  "Ensure we get two press/release pairs for button 2.\n");

    const unsigned int BTN1_PRESS   = 0x0080000 | 0x00100000 | 1; /* btn 1 press */
    const unsigned int BTN2_PRESS   = 0x0080000 | 0x00100000 | 2; /* btn 1 press */
    const unsigned int BTN2_RELEASE = 0x0080000 | 0x00000000 | 2; /* btn 1 release */
    const std::string propname = "Wacom button action 0";

    ::Display *dpy = Display();

    ASSERT_PROPERTY(unsigned int, prop, dpy, stylus_id, propname);
    ASSERT_NE(prop.prop, (Atom)None);
    ASSERT_EQ(prop.nitems, 1U);
    ASSERT_EQ(prop.At(0), BTN1_PRESS);

               /* AC_BUTTON  AC_KEYBTNPRESS */
    prop.Set(0, BTN2_PRESS);
    prop.Set(1, BTN2_RELEASE);
    prop.Set(2, BTN2_PRESS);
    prop.Set(3, BTN2_RELEASE);
    prop.Update();

    XSelectInput(dpy, DefaultRootWindow(dpy), ButtonPressMask | ButtonReleaseMask);

    TipDown();

    ASSERT_EVENT(XEvent, press1, dpy, ButtonPress);
    ASSERT_EQ(press1->xbutton.button, 2U);
    ASSERT_EVENT(XEvent, release1, dpy, ButtonRelease);
    ASSERT_EQ(release1->xbutton.button, 2U);
    ASSERT_EVENT(XEvent, press2, dpy, ButtonPress);
    ASSERT_EQ(press2->xbutton.button, 2U);
    ASSERT_EVENT(XEvent, release2, dpy, ButtonRelease);
    ASSERT_EQ(release2->xbutton.button, 2U);

    TipUp();
    XSync(dpy, False);
    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_F(WacomPropertyTest, ButtonActionInvalidFormat)
{
    XORG_TESTCASE("Try to change button 0 action property to a different format.\n"
                  "Expect error\n");

    const std::string propname = "Wacom button action ";

    ::Display *dpy = Display();

    ASSERT_PROPERTY(Atom, btnaction_prop, dpy, stylus_id, "Wacom Button Actions");

    int data[1] = {0};

    for (unsigned int i = 0; i < btnaction_prop.nitems; i++) {
        std::stringstream ss;
        ss << propname << ButtonNumber(i);

        SetErrorTrap(dpy);
        XITProperty<int> prop_8(dpy, stylus_id, ss.str(), XA_INTEGER, 8, 1, data);
        ASSERT_ERROR(ReleaseErrorTrap(dpy), BadMatch);

        SetErrorTrap(dpy);
        XITProperty<int> prop_16(dpy, stylus_id, ss.str(), XA_INTEGER, 16, 1, data);
        ASSERT_ERROR(ReleaseErrorTrap(dpy), BadMatch);
    }
}

TEST_F(WacomPropertyTest, ButtonActionInvalidType)
{
    XORG_TESTCASE("Try to change button 0 action property to a different type.\n"
                  "Expect error\n");

    const std::string propname = "Wacom button action ";

    ::Display *dpy = Display();

    ASSERT_PROPERTY(Atom, btnaction_prop, dpy, stylus_id, "Wacom Button Actions");

    int data[1] = {0};

    /* All built-in types */
    for (unsigned int i = XA_PRIMARY; i < XA_LAST_PREDEFINED; i++) {
        if (i == XA_INTEGER)
            continue;

        for (unsigned int j = 0; j < btnaction_prop.nitems; j++) {
            std::stringstream ss;
            ss << propname << ButtonNumber(j);

            SetErrorTrap(dpy);
            XITProperty<int> prop_str(dpy, stylus_id, ss.str(), i, 32, 1, data);
            ASSERT_ERROR(ReleaseErrorTrap(dpy), BadMatch);
        }
    }
}

TEST_F(WacomPropertyTest, ButtonActionHighButtonValue)
{
    XORG_TESTCASE("Try to change button 0 action property to a button > 32.\n"
                  "Expect success\n");
    const unsigned int BTN100_PRESS   = 0x0080000 | 0x00100000 | 100; /* btn 1 press */

    const std::string propname = "Wacom button action 0";

    ::Display *dpy = Display();

    SetErrorTrap(dpy);
    XITProperty<unsigned int> prop_str(dpy, stylus_id, propname, XA_INTEGER, 32, 1, &BTN100_PRESS);
    ASSERT_FALSE(ReleaseErrorTrap(dpy));
}

TEST_F(WacomPropertyTest, ButtonActionPropertySetToNone)
{
    XORG_TESTCASE("Change button X action property value to None\n"
                  "Ensure this triggers an error\n");

    ::Display *dpy = Display();
    ASSERT_PROPERTY(Atom, btnactions, dpy, stylus_id, "Wacom Button Actions");

    for (unsigned int i = 0; i < btnactions.nitems; i++) {
        const std::string propname = "Wacom button action ";
        std::stringstream ss;
        ss << propname << ButtonNumber(i);

        ASSERT_PROPERTY(unsigned int, prop, dpy, stylus_id, ss.str());
        prop.Resize(1);
        prop.Set(0, None);

        SetErrorTrap(dpy);
        prop.Update();
        ASSERT_ERROR(ReleaseErrorTrap(dpy), BadValue);
    }
}

TEST_F(WacomPropertyTest, ButtonActionPropertyUnset)
{
    XORG_TESTCASE("Change Wacom Button Actions property value to None for a button\n"
                  "Ensure this does not generate an error\n");

    const std::string propname = "Wacom Button Actions";

    ::Display *dpy = Display();

    ASSERT_PROPERTY(unsigned int, prop, dpy, stylus_id, propname);
    for (unsigned int i = 0; i < prop.nitems; i++) {
        ASSERT_PROPERTY(unsigned int, prop, dpy, stylus_id, propname);
        prop.Set(i, None);

        SetErrorTrap(dpy);
        prop.Update();
        ASSERT_FALSE(ReleaseErrorTrap(dpy));
    }
}

TEST_F(WacomPropertyTest, ButtonActionKeyPress)
{
    XORG_TESTCASE("Change button action to a key press.\n"
                  "Press phys button.\n"
                  "Ensure key press is received\n"
                  "Release phys button.\n"
                  "Ensure key release is received\n");

    unsigned int KEY_A_KEYCODE = 38;
    unsigned int KEY_A_PRESS   = 0x0010000 | 0x00100000 | KEY_A_KEYCODE;
    const std::string propname = "Wacom button action 0";

    ::Display *dpy = Display();

    ASSERT_PROPERTY(unsigned int, prop, dpy, stylus_id, propname);
    prop.Resize(1);
    prop.Set(0, KEY_A_PRESS);
    prop.Update();

    XSelectInput(dpy, DefaultRootWindow(dpy), KeyPressMask | KeyReleaseMask |
                                              ButtonPressMask | ButtonReleaseMask);

    TipDown();
    ASSERT_EVENT(XEvent, press, dpy, KeyPress);
    ASSERT_EQ(press->xkey.keycode, KEY_A_KEYCODE);
    ASSERT_TRUE(NoEventPending(dpy));

    TipUp();
    ASSERT_EVENT(XEvent, release, dpy, KeyRelease);
    ASSERT_EQ(release->xkey.keycode, KEY_A_KEYCODE);
    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_F(WacomPropertyTest, ButtonActionKeyPressRelease)
{
    XORG_TESTCASE("Change button action to a key press and release.\n"
                  "Press phys button.\n"
                  "Ensure key press is received\n"
                  "Ensure key release is received\n"
                  "Release phys button.\n"
                  "Ensure no more events are waiting\n");

    unsigned int KEY_A_KEYCODE = 38;
    unsigned int KEY_A_PRESS   = 0x0010000 | 0x00100000 | KEY_A_KEYCODE;
    unsigned int KEY_A_RELEASE = 0x0010000 |              KEY_A_KEYCODE;
    const std::string propname = "Wacom button action 0";

    ::Display *dpy = Display();

    ASSERT_PROPERTY(unsigned int, prop, dpy, stylus_id, propname);
    prop.Resize(2);
    prop.Set(0, KEY_A_PRESS);
    prop.Set(1, KEY_A_RELEASE);
    prop.Update();

    XSelectInput(dpy, DefaultRootWindow(dpy), KeyPressMask | KeyReleaseMask |
                                              ButtonPressMask | ButtonReleaseMask);

    TipDown();
    ASSERT_EVENT(XEvent, press, dpy, KeyPress);
    ASSERT_EQ(press->xkey.keycode, KEY_A_KEYCODE);
    ASSERT_EQ(XPending(dpy), 1);

    ASSERT_EVENT(XEvent, release, dpy, KeyRelease);
    ASSERT_EQ(release->xkey.keycode, KEY_A_KEYCODE);
    ASSERT_TRUE(NoEventPending(dpy));

    TipUp();
    XSync(dpy, False);
    ASSERT_TRUE(NoEventPending(dpy));
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
