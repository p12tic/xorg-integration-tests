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
#include "xit-server.h"
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

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
