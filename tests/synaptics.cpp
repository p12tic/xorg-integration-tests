#include <stdexcept>
#include <map>
#include <utility>
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

#include "input-driver-test.h"
#include "helpers.h"

/**
 * Synaptics driver test for touchpad devices. This class uses a traditional
 * touchpad device.
 */
class SynapticsDriverTest : public InputDriverTest {
public:
    /**
     * Initializes a standard touchpad device.
     */
    virtual void SetUp() {
        dev = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad.desc"
                    )
                );
        InputDriverTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single synaptics CorePointer device based on
     * the evemu device. Options enabled: tapping (1 finger), two-finger
     * vertical scroll.
     */
    virtual void SetUpConfigAndLog(const std::string &param) {
        server.SetOption("-logfile", "/tmp/Xorg-synaptics-driver-SynPS2.log");
        server.SetOption("-config", "/tmp/synaptics-SynPS2.conf");

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("synaptics", "--device--",
                               "Option \"Protocol\"            \"event\"\n"
                               "Option \"CorePointer\"         \"on\"\n"
                               "Option \"TapButton1\"          \"1\"\n"
                               "Option \"GrabEventDevice\"     \"0\"\n"
                               "Option \"FastTaps\"            \"1\"\n"
                               "Option \"VertTwoFingerScroll\" \"1\"\n"
                               "Option \"Device\"              \"" + dev->GetDeviceNode() + "\"\n");
        config.WriteConfig("/tmp/synaptics-SynPS2.conf");
    }

protected:
    std::auto_ptr<xorg::testing::evemu::Device> dev;
};

TEST_F(SynapticsDriverTest, DevicePresent)
{
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));

    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(Display(), XIAllDevices, &ndevices);

    /* VCP, VCK, VC XTEST keyboard, VC XTEST pointer, default keyboard, --device-- */
    ASSERT_EQ(6, ndevices);
    bool found = false;
    while(ndevices--) {
        if (strcmp(info[ndevices].name, "--device--") == 0) {
            ASSERT_EQ(found, false) << "Duplicate device" << std::endl;
            found = true;
        }
    }
    ASSERT_EQ(found, true);
    XIFreeDeviceInfo(info);
}

void check_buttons_event(::Display *display,
                        xorg::testing::evemu::Device *dev,
                        const std::string& events_path,
                        int button,
                        int expect_nevents) {

    dev->Play(RECORDINGS_DIR "touchpads/" + events_path);
    XSync(display, False);

    ASSERT_NE(XPending(display), 0) << "No event pending" << std::endl;
    XEvent btn;
    int nevents = 0;
    while(XPending(display)) {
        XNextEvent(display, &btn);

        ASSERT_EQ(btn.xbutton.type, ButtonPress);
        ASSERT_EQ(btn.xbutton.button, button);

        XNextEvent(display, &btn);
        ASSERT_EQ(btn.xbutton.type, ButtonRelease);
        ASSERT_EQ(btn.xbutton.button, button);

        nevents++;
    }

    ASSERT_EQ(expect_nevents, nevents);
}

TEST_F(SynapticsDriverTest, ScrollWheel)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    /* the server takes a while to start up bust the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());
    check_buttons_event(Display(), dev.get(), "SynPS2-Synaptics-TouchPad-two-finger-scroll-up.events", 4, 17);
    check_buttons_event(Display(), dev.get(), "SynPS2-Synaptics-TouchPad-two-finger-scroll-down.events", 5, 10);
}

TEST_F(SynapticsDriverTest, TapEvent)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    /* the server takes a while to start up bust the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());
    check_buttons_event(Display(), dev.get(), "SynPS2-Synaptics-TouchPad-tap.events", 1, 1);
}

void check_drag_event(::Display *display,
                      xorg::testing::evemu::Device *dev,
                      const std::string& events_path,
                      int expect_npress,
                      int expect_nmotion,
                      int expect_nrelease) {

    dev->Play(RECORDINGS_DIR "touchpads/" + events_path);
    XSync(display, False);

    ASSERT_NE(XPending(display), 0) << "No event pending" << std::endl;
    XEvent ev;
    int nevents = 0;

    while (XCheckMaskEvent (display, ButtonPressMask, &ev))
        nevents++;
    ASSERT_EQ(expect_npress, nevents);
    ASSERT_EQ(ev.xbutton.button, 1);

    nevents = 0;
    while (XCheckMaskEvent (display, PointerMotionMask, &ev))
        nevents++;
    ASSERT_EQ(expect_nmotion, nevents);

    nevents = 0;
    while (XCheckMaskEvent (display, ButtonReleaseMask, &ev))
        nevents++;
    ASSERT_EQ(expect_nrelease, nevents);
    ASSERT_EQ(ev.xbutton.button, 1);

    while(XPending(display))
        XNextEvent(display, &ev);
}

TEST_F(SynapticsDriverTest, TapAndDragEvent)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    /* the server takes a while to start up bust the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());
    /* 1 press, 30 moves then 1 release is what we expect */
    check_drag_event(Display(), dev.get(), "SynPS2-Synaptics-TouchPad-tap-and-move.events", 1, 30, 1);
}

/**
 * Synaptics driver test for Bug 53037 - Device coordinate scaling breaks
 * XWarpPointer requests
 *
 * If a device with a device coordinate range is the lastSlave, a
 * WarpPointer request on some coordinates may end up on different pixels
 * than requested. The server scales from screen to device coordinates, then
 * back to screen - a rounding error may then change to the wrong pixel.
 *
 * https://bugs.freedesktop.org/show_bug.cgi?id=53037
 *
 * Takes a pair of integers, not used by the class itself.
 */
class SynapticsWarpTest : public SynapticsDriverTest,
                          public ::testing::WithParamInterface<std::pair<int, int> >{
};

TEST_P(SynapticsWarpTest, WarpScaling)
{
    SCOPED_TRACE("https://bugs.freedesktop.org/show_bug.cgi?id=53037");

    /* Play one event so this device is lastSlave */
    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-move.events");

    int x = GetParam().first,
        y = GetParam().second;

    XWarpPointer(Display(), 0, DefaultRootWindow(Display()), 0, 0, 0, 0, x, y);

    Window root, child;
    int rx, ry, wx, wy;
    unsigned int mask;

    XQueryPointer(Display(), DefaultRootWindow(Display()), &root, &child,
                  &rx, &ry, &wx, &wy, &mask);
    ASSERT_EQ(root, DefaultRootWindow(Display()));
    /* Warping to negative should clip to 0 */
    ASSERT_EQ(rx, std::max(x, 0));
    ASSERT_EQ(ry, std::max(y, 0));
}

INSTANTIATE_TEST_CASE_P(, SynapticsWarpTest,
                        ::testing::Values(std::make_pair(0, 0),
                                          std::make_pair(1, 1),
                                          std::make_pair(2, 2),
                                          std::make_pair(200, 200),
                                          std::make_pair(-1, -1)
                            ));

/**
 * Synaptics driver test for clickpad devices.
 */
class SynapticsDriverClickpadTest : public InputDriverTest {
public:
    /**
     * Initializes a clickpad, as the one found on the Lenovo x220t.
     */
    virtual void SetUp() {
        dev = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.desc"
                    )
                );
        InputDriverTest::SetUp();
    }

    /**
     * Set up a single clickpad CorePointer device.
     */
    virtual void SetUpConfigAndLog(const std::string &param) {
        server.SetOption("-logfile", "/tmp/Xorg-synaptics-driver-clickpad.log");
        server.SetOption("-config", "/tmp/synaptics-clickpad.conf");

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("synaptics", "--device--",
                               "Option \"CorePointer\"         \"on\"\n"
                               "Option \"GrabEventDevice\"     \"0\"\n"
                               "Option \"Device\"              \"" + dev->GetDeviceNode() + "\"\n");
        config.WriteConfig("/tmp/synaptics-clickpad.conf");
    }

protected:
    std::auto_ptr<xorg::testing::evemu::Device> dev;
};

TEST_F(SynapticsDriverClickpadTest, ClickpadProperties)
{
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    Atom clickpad_prop = XInternAtom(Display(), "Synaptics ClickPad", True);
    ASSERT_NE(clickpad_prop, None);

    Status status;
    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *data;

    status = XIGetProperty(Display(), deviceid, clickpad_prop, 0, 1, False,
                           XIAnyPropertyType, &type, &format, &nitems,
                           &bytes_after, &data);

    ASSERT_EQ(status, Success);
    ASSERT_EQ(format, 8);
    ASSERT_EQ(nitems, 1);
    ASSERT_EQ(bytes_after, 0);
    ASSERT_EQ(data[0], 1);
    free(data);

    /* This option is assigned by the xorg.conf.d, it won't activate for
       xorg.conf devices. */
    Atom softbutton_props = XInternAtom(Display(), "Synaptics Soft Button Areas", True);
    ASSERT_NE(softbutton_props, None);

    status = XIGetProperty(Display(), deviceid, softbutton_props, 0, 8, False,
                           XIAnyPropertyType, &type, &format, &nitems,
                           &bytes_after, &data);

    ASSERT_EQ(status, Success);
    ASSERT_EQ(format, 32);
    ASSERT_EQ(nitems, 8);
    ASSERT_EQ(bytes_after, 0);

    int32_t *buttons = reinterpret_cast<int32_t*>(data);
    ASSERT_EQ(buttons[0], 0);
    ASSERT_EQ(buttons[1], 0);
    ASSERT_EQ(buttons[2], 0);
    ASSERT_EQ(buttons[3], 0);
    ASSERT_EQ(buttons[4], 0);
    ASSERT_EQ(buttons[5], 0);
    ASSERT_EQ(buttons[6], 0);
    ASSERT_EQ(buttons[7], 0);

    free(data);

}

/**
 * Synaptics driver test for clickpad devices with the SoftButtonArea option
 * set.
 */
class SynapticsDriverClickpadSoftButtonsTest : public SynapticsDriverClickpadTest {
public:
    /**
     * Sets up a single CorePointer synaptics clickpad device with the
     * SoftButtonArea option set to 50% left/right, 82% from the top.
     */
    virtual void SetUpConfigAndLog(const std::string &param) {
        server.SetOption("-logfile", "/tmp/Xorg-synaptics-driver-clickpad-softbuttons.log");
        server.SetOption("-config", "/tmp/synaptics-clickpad-softbuttons.conf");

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("synaptics", "--device--",
                               "Option \"CorePointer\"         \"on\"\n"
                               "Option \"GrabEventDevice\"     \"0\"\n"
                               "Option \"SoftButtonAreas\"     \"50% 0 82% 0 0 0 0 0\"\n"
                               "Option \"Device\"              \"" + dev->GetDeviceNode() + "\"\n");
        config.WriteConfig("/tmp/synaptics-clickpad-softbuttons.conf");
    }
};

TEST_F(SynapticsDriverClickpadSoftButtonsTest, LeftClick)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.left-phys-click.events");

    XEvent btn;
    ASSERT_NE(XPending(Display()), 0) << "No event pending" << std::endl;
    XNextEvent(Display(), &btn);

    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 1);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, 1);

    XSync(Display(), True);
}

TEST_F(SynapticsDriverClickpadSoftButtonsTest, RightClick)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XSync(Display(), False);

    dev->Play(RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.right-phys-click.events");

    XEvent btn;
    ASSERT_NE(XPending(Display()), 0) << "No event pending" << std::endl;
    XNextEvent(Display(), &btn);

    ASSERT_EQ(btn.xbutton.type, ButtonPress);
    ASSERT_EQ(btn.xbutton.button, 3);

    XNextEvent(Display(), &btn);
    ASSERT_EQ(btn.xbutton.type, ButtonRelease);
    ASSERT_EQ(btn.xbutton.button, 3);

    XSync(Display(), True);
}

TEST(SynapticsClickPad, HotPlugSoftButtons)
{
    std::auto_ptr<xorg::testing::evemu::Device> dev;

    dev = std::auto_ptr<xorg::testing::evemu::Device>(
            new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad-Clickpad.desc"
                )
            );

    XOrgConfig config;
    xorg::testing::XServer server;

    config.SetAutoAddDevices(true);
    config.AddDefaultScreenWithDriver();
    StartServer("synaptics-clickpad-softbuttons", server, config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    int deviceid;
    int ndevices = FindInputDeviceByName(dpy, "SynPS/2 Synaptics TouchPad", &deviceid);
    ASSERT_GT(ndevices, 0);
    EXPECT_EQ(ndevices, 1) << "More than one touchpad found, cannot "
                              "guarantee right behaviour.";

    Atom clickpad_prop = XInternAtom(dpy, "Synaptics ClickPad", True);
    ASSERT_NE(clickpad_prop, None);

    Status status;
    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *data;

    status = XIGetProperty(dpy, deviceid, clickpad_prop, 0, 1, False,
                           XIAnyPropertyType, &type, &format, &nitems,
                           &bytes_after, &data);

    ASSERT_EQ(status, Success);
    ASSERT_EQ(format, 8);
    ASSERT_EQ(nitems, 1);
    ASSERT_EQ(bytes_after, 0);
    ASSERT_EQ(data[0], 1);
    free(data);

    /* This option is assigned by the xorg.conf.d, it won't activate for
       xorg.conf devices. */
    Atom softbutton_props = XInternAtom(dpy, "Synaptics Soft Button Areas", True);
    ASSERT_NE(softbutton_props, None);

    status = XIGetProperty(dpy, deviceid, softbutton_props, 0, 8, False,
                           XIAnyPropertyType, &type, &format, &nitems,
                           &bytes_after, &data);

    ASSERT_EQ(status, Success);
    ASSERT_EQ(format, 32);
    ASSERT_EQ(nitems, 8);
    ASSERT_EQ(bytes_after, 0);

    int32_t *buttons = reinterpret_cast<int32_t*>(data);
    ASSERT_EQ(buttons[0], 3472);
    ASSERT_EQ(buttons[1], 0);
    ASSERT_EQ(buttons[2], 3900);
    ASSERT_EQ(buttons[3], 0);
    ASSERT_EQ(buttons[4], 0);
    ASSERT_EQ(buttons[5], 0);
    ASSERT_EQ(buttons[6], 0);
    ASSERT_EQ(buttons[7], 0);

    free(data);

    config.RemoveConfig();
    server.RemoveLogFile();
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
