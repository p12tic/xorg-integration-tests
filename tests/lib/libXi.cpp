#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <linux/input.h>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/extensions/XI2.h>
#include <X11/extensions/XInput2.h>

#include <input-driver-test.h>
#include <device-interface.h>

#include "helpers.h"

/**
 * Test for libXi-related bugs. Initialises a single evdev pointer device ready
 * for uinput-events.
 */
class libXiTest : public InputDriverTest,
                  public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device with two wheels.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        InputDriverTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CorePointer device based on
     * the evemu device.
     */
    virtual void SetUpConfigAndLog(const std::string &param) {
        InitDefaultLogFiles(server, &config);

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
    ASSERT_EQ(dev->display, dpy) << "https://bugzilla.redhat.com/show_bug.cgi?id=804907";

    XFreeEventData(dpy, &ev.xcookie);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

