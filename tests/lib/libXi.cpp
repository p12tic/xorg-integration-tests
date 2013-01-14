#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <linux/input.h>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/extensions/XI2.h>
#include <X11/extensions/XInput2.h>

#include <xit-server-input-test.h>
#include <device-interface.h>

#include "helpers.h"
#include "xit-event.h"

/**
 * Test for libXi-related bugs. Initialises a single evdev pointer device ready
 * for uinput-events.
 */
class libXiTest : public XITServerInputTest,
                  public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device with two wheels.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        XITServerInputTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CorePointer device based on
     * the evemu device.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "keyboard-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
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

    dev->PlayOne(EV_REL, REL_X, -1, true);

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

class libXiTouchTest : public libXiTest {
public:
    /**
     * Initializes a standard mouse device with two wheels.
     */
    virtual void SetUp() {
        SetDevice("tablets/N-Trig-MultiTouch.desc");
        XITServerInputTest::SetUp();
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

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_update.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

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

