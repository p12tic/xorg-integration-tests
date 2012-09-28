#include <stdexcept>

#include "device-interface.h"

#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

/**
 * A test fixture for testing the XInput 2.x extension.
 *
 * @tparam The XInput 2.x minor version
 */
class XInput2Test : public xorg::testing::Test,
                    public DeviceInterface,
                    public ::testing::WithParamInterface<int> {
protected:
    virtual void SetUp()
    {
        SetDevice("tablets/N-Trig-MultiTouch.desc");

        ASSERT_NO_FATAL_FAILURE(xorg::testing::Test::SetUp());

        int event_start;
        int error_start;

        ASSERT_TRUE(XQueryExtension(Display(), "XInputExtension", &xi2_opcode_,
                                    &event_start, &error_start));

        int major = 2;
        int minor = GetParam();

        ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));
    }

    int xi2_opcode_;
};

TEST_P(XInput2Test, XIQueryPointerTouchscreen)
{
    SCOPED_TRACE("\n"
                 "XIQueryPointer for XInput 2.1 and earlier should report the\n"
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

    dev->Play(RECORDINGS_DIR "tablets/N-Trig MultiTouch.touch_1_begin.events");

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode_,
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
    SCOPED_TRACE("When a device is disabled, any physically active touches\n"
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

    dev->Play(RECORDINGS_DIR "tablets/N-Trig MultiTouch.touch_1_begin.events");

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode_,
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
    ASSERT_TRUE(XChangeDeviceControl(Display(), xdevice, DEVICE_ENABLE,
                                     control));
    XCloseDevice(Display(), xdevice);
    XFlush(Display());

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode_,
                                                           XI_TouchEnd));
}
#endif

INSTANTIATE_TEST_CASE_P(, XInput2Test, ::testing::Range(0, 3));
