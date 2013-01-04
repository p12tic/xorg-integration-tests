#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XTest.h>

#include "xorg-conf.h"
#include "xit-server-input-test.h"
#include "device-interface.h"
#include "xit-server.h"
#include "xit-event.h"
#include "helpers.h"

using namespace xorg::testing;

static void disable_device_property(Display *dpy, std::string name)
{
    Atom prop = XInternAtom(dpy, "Device Enabled", True);
    ASSERT_NE(prop, (Atom)None);

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, name, &deviceid), 1);
    unsigned char data = 0;
    XIChangeProperty(dpy, deviceid, prop, XA_INTEGER, 8,
                     PropModeReplace, &data, 1);
    XSync(dpy, False);
}

TEST(XTest, DisabledDevicesProperty)
{
    XORG_TESTCASE("Disabling an XTest device through properties "
                 "should not be possible\n"
                 "https://bugs.freedesktop.org/show_bug.cgi?id=56380");

    XITServer server;
    XOrgConfig config;
    config.AddDefaultScreenWithDriver();
    server.Start(config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy);

    SetErrorTrap(dpy);
    disable_device_property(dpy, "Virtual core pointer");
    ASSERT_ERROR(ReleaseErrorTrap(dpy), BadAccess);

    SetErrorTrap(dpy);
    disable_device_property(dpy, "Virtual core keyboard");
    ASSERT_ERROR(ReleaseErrorTrap(dpy), BadAccess);

    SetErrorTrap(dpy);
    disable_device_property(dpy, "Virtual core XTEST pointer");
    ASSERT_ERROR(ReleaseErrorTrap(dpy), BadAccess);

    SetErrorTrap(dpy);
    disable_device_property(dpy, "Virtual core XTEST keyboard");
    ASSERT_ERROR(ReleaseErrorTrap(dpy), BadAccess);

    /* if disabled, this will crash the server */
    XTestFakeButtonEvent(dpy, 1, 1, 0);
    XTestFakeButtonEvent(dpy, 1, 2, 0);
    XTestFakeKeyEvent(dpy, 64, 1, 0);
    XTestFakeKeyEvent(dpy, 64, 2, 0);
    XSync(dpy, False);

    config.RemoveConfig();
}

static void disable_device_devctl(Display *dpy, std::string name)
{
    XDeviceEnableControl ctl;
    ctl.control = DEVICE_ENABLE;
    ctl.length = sizeof(XDeviceEnableControl);
    ctl.enable = 0;

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy, name, &deviceid), 1);
    XDevice *dev = XOpenDevice(dpy, deviceid);
    ASSERT_TRUE(dev);
    XChangeDeviceControl(dpy, dev, DEVICE_ENABLE,
                         reinterpret_cast<XDeviceControl*>(&ctl));
    XSync(dpy, False);
}

TEST(XTest, DisabledDevicesCtl)
{
    XORG_TESTCASE("Disabling an XTest device through device "
                  "controls should not possible\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=56380");

    XITServer server;
    XOrgConfig config;
    config.AddDefaultScreenWithDriver();
    server.Start(config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy);

    XSync(dpy, False);

    SetErrorTrap(dpy);
    disable_device_devctl(dpy, "Virtual core XTEST pointer");
    ASSERT_ERROR(ReleaseErrorTrap(dpy), BadMatch);

    SetErrorTrap(dpy);
    disable_device_devctl(dpy, "Virtual core XTEST keyboard");
    ASSERT_ERROR(ReleaseErrorTrap(dpy), BadMatch);

    /* if disabled, this will crash the server */
    XTestFakeButtonEvent(dpy, 1, 1, 0);
    XTestFakeButtonEvent(dpy, 1, 2, 0);
    XTestFakeKeyEvent(dpy, 64, 1, 0);
    XTestFakeKeyEvent(dpy, 64, 2, 0);
    XSync(dpy, False);

    config.RemoveConfig();
}

class XTestPhysicalDeviceTest : public XITServerInputTest,
                                public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        XITServerInputTest::SetUp();
    }

    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "kbd-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }
};

TEST_F(XTestPhysicalDeviceTest, ReleaseXTestOnPhysicalRelease)
{
    XORG_TESTCASE("Create a pointer device.\n"
                  "Post button press through pointer device.\n"
                  "Post button press through XTest device.\n"
                  "Post button release through pointer device.\n"
                  "Expect ButtonRelease, expect button state up\n");

    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    XSelectInput(dpy, DefaultRootWindow(dpy), ButtonPressMask|ButtonReleaseMask);

    dev->PlayOne(EV_KEY, BTN_LEFT, 1, True);

    ASSERT_EVENT(XButtonEvent, press, dpy, ButtonPress);
    ASSERT_EQ(press->button, 1U);

    XTestFakeButtonEvent(dpy, 1, 1, 0);

    dev->PlayOne(EV_KEY, BTN_LEFT, 0, True);

    ASSERT_EVENT(XButtonEvent, release, dpy, ButtonRelease);
    ASSERT_EQ(release->button, 1U);

    Window root, child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;
    XQueryPointer(dpy, DefaultRootWindow(dpy), &root, &child,
                  &root_x, &root_y, &win_x, &win_y, &mask);
    ASSERT_EQ(mask & Button1Mask, 0U);
}
