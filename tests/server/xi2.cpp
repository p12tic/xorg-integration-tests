#include <stdexcept>

#include "helpers.h"
#include "device-interface.h"
#include "input-driver-test.h"

#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

/**
 * A test fixture for testing the XInput 2.x extension.
 *
 * @tparam The XInput 2.x minor version
 */
class XInput2Test : public InputDriverTest,
                    public DeviceInterface,
                    public ::testing::WithParamInterface<int> {
protected:
    virtual void SetUp() {
        SetDevice("tablets/N-Trig-MultiTouch.desc");
        InputDriverTest::SetUp();
    }

    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.SetAutoAddDevices(true);
        config.WriteConfig();
    }

    virtual int RegisterXI2(int major, int minor)
    {
        return InputDriverTest::RegisterXI2(2, GetParam());
    }
};

TEST_P(XInput2Test, XITouchscreenPointerEmulation)
{
    XORG_TESTCASE("When an initial touch is made, any movement of the pointer\n"
                  "should be raised as if button 1 is being held. After the\n"
                  "touch is released, further movement should have button 1\n"
                  "released.\n");

    ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(Display(), "N-Trig MultiTouch"));

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_HierarchyChanged);
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

TEST_P(XInput2Test, EmulatedButtonMaskOnTouchBeginEndCore)
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

TEST_P(XInput2Test, EmulatedButtonMaskOnTouchBeginEndXI2)
{
    XORG_TESTCASE("Select for XI_Motion and XI_ButtonPress/Release events on the root window.\n"
                  "Create a pointer-emulating touch event.\n"
                  "Expect a Motion event before the button press.\n"
                  "The button mask in the motion event must be 0.\n"
                  "The button mask in the button press event must be 0.\n"
                  "The button mask in the button release event must be set for button 1.");

    XIEventMask mask;
    mask.deviceid = 2; /* VCP */
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

TEST_P(XInput2Test, XIQueryPointerTouchscreen)
{
    XORG_TESTCASE("XIQueryPointer for XInput 2.1 and earlier should report the\n"
                  "first button pressed if a touch is physically active. For \n"
                  "XInput 2.2 and later clients, the first button should not be\n"
                  "reported.");
    XIEventMask mask;
    mask.deviceid = XIAllDevices;
    mask.mask_len = XIMaskLen(XI_HierarchyChanged);
    mask.mask = reinterpret_cast<unsigned char*>(calloc(mask.mask_len, 1));
    XISetMask(mask.mask, XI_HierarchyChanged);

    ASSERT_EQ(Success,
              XISelectEvents(Display(), DefaultRootWindow(Display()), &mask,
                             1));

    mask.deviceid = XIAllMasterDevices;
    XIClearMask(mask.mask, XI_HierarchyChanged);
    XISetMask(mask.mask, XI_ButtonPress);

    ASSERT_EQ(Success,
              XISelectEvents(Display(), DefaultRootWindow(Display()), &mask,
                             1));

    free(mask.mask);

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
TEST_P(XInput2Test, DisableDeviceEndTouches)
{
    XORG_TESTCASE("When a device is disabled, any physically active touches\n"
                  "should end.");
    /* This is an XInput 2.2 and later test only */
    if (GetParam() < 2)
        return;

    XIEventMask mask;
    mask.deviceid = XIAllDevices;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = reinterpret_cast<unsigned char*>(calloc(mask.mask_len, 1));
    XISetMask(mask.mask, XI_HierarchyChanged);

    ASSERT_EQ(Success,
              XISelectEvents(Display(), DefaultRootWindow(Display()), &mask,
                             1));

    mask.deviceid = XIAllMasterDevices;
    XIClearMask(mask.mask, XI_HierarchyChanged);
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    ASSERT_EQ(Success,
              XISelectEvents(Display(), DefaultRootWindow(Display()), &mask,
                             1));

    free(mask.mask);

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
 * Test class for testing
 * @tparam The device ID
 */
class XInput2TouchSelectionTest : public XInput2Test
{
protected:
    virtual int RegisterXI2(int major, int minor)
    {
        return InputDriverTest::RegisterXI2(2, 2);
    }
};

static bool error_handler_triggered;

/* ASSERT_*() macros can only be used in void functions, hence the
   indirection here */
static void _error_handler(XErrorEvent *ev) {
    ASSERT_EQ((int)ev->error_code, BadAccess);
    error_handler_triggered = true;
}
static int error_handler(Display *dpy, XErrorEvent *ev) {
    _error_handler(ev);
    return 0;
}
static void _fail_error_handler(XErrorEvent *ev) {
    FAIL() << "X protocol error: errorcode: " << (int)ev->error_code <<
    " on request " << (int)ev->request_code << " minor " << (int)ev->minor_code;
}
static int fail_error_handler(Display *dpy, XErrorEvent *ev) {
    _fail_error_handler(ev);
    return 0;
}

TEST_P(XInput2TouchSelectionTest, TouchSelectionConflicts)
{
    XORG_TESTCASE("If client A has a selection on a device,\n"
                  "client B selecting on the same device returns BadAccess.\n"
                  "If client A has a selection on XIAll(Master)Devices, \n"
                  "selecting on the same or a specific device returns"
                  "BadAccess\n");

    unsigned char m[XIMaskLen(XI_TouchEnd)] = {0};
    XIEventMask mask;
    mask.mask = m;
    mask.mask_len = sizeof(m);

    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    int clientA_deviceid = GetParam();

    /* client A */
    mask.deviceid = clientA_deviceid;
    XSetErrorHandler(fail_error_handler);
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy2);

    XISelectEvents(dpy2, DefaultRootWindow(dpy2), &mask, 1);
    XSync(dpy2, False);

    XSetErrorHandler(error_handler);

    /* covers XIAllDevices, XIAllMasterDevices and VCP */
    for (int clientB_deviceid = 0; clientB_deviceid < 3; clientB_deviceid++) {
        error_handler_triggered = false;
        mask.deviceid = clientB_deviceid;
        XISelectEvents(Display(), DefaultRootWindow(Display()), &mask, 1);
        XSync(Display(), False);
        ASSERT_EQ(error_handler_triggered,
                  (clientA_deviceid <= clientB_deviceid))
                  << "failed for " << clientA_deviceid << "/" << clientB_deviceid;
    }
}
INSTANTIATE_TEST_CASE_P(, XInput2TouchSelectionTest, ::testing::Range(0, 3));


TEST_P(XInput2Test, TouchEventsButtonState)
{
    if (GetParam() < 2)
        return;

    XORG_TESTCASE("Select for touch events on the root window.\n"
                  "Create a pointer-emulating touch event.\n"
                  "The button mask in the touch begin event must be 0.\n"
                  "The button mask in the touch update events must be set for button 1.\n"
                  "The button mask in the touch end event must be set for button 1.");

    XIEventMask mask;
    mask.deviceid = 2; /* VCP */
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
    EXPECT_TRUE(XIMaskIsSet(end->buttons.mask, 1)) << "ButtonRelease must have button 1 down";
    for (int i = 0; i < end->buttons.mask_len * 8; i++) {
        if (i == 1)
            continue;
        EXPECT_FALSE(XIMaskIsSet(end->buttons.mask, i)) << "mask down for button " << i;
    }

    XFreeEventData(Display(), &ev.xcookie);
}

#endif /* HAVE_XI22 */

INSTANTIATE_TEST_CASE_P(, XInput2Test, ::testing::Range(0, 3));

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
