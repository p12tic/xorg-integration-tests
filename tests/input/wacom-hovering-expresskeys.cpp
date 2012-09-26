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
#include <X11/Xutil.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <xorg/wacom-properties.h>
#include <xorg/xserver-properties.h>
#include <unistd.h>

#include "input-driver-test.h"
#include "device-interface.h"
#include "helpers.h"

/**
 * Wacom driver test class. This class takes a struct Tablet that defines
 * which device should be initialised.
 */
class WacomHoveringTest : public InputDriverTest,
                          public DeviceInterface {
public:
    /**
     * Initializes a standard tablet device.
     */
    virtual void SetUp() {
        SetDevice("tablets/Wacom-Intuos5-touch-M-Pen.desc");
        InputDriverTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single mouse CorePointer device based on
     * the evemu device.
     */
    virtual void SetUpConfigAndLog(const std::string &param) {
        server.SetOption("-logfile", "/tmp/Xorg-wacom-hovering-test.log");
        server.SetOption("-config", "/tmp/wacom-hovering-test.conf");

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("wacom", "Stylus",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Type\"        \"stylus\"\n"
                               "Option \"DebugLevel\"  \"12\"\n"
                               "Option \"CommonDBG\"   \"12\"\n"
                               "Option \"Device\"      \"" + dev->GetDeviceNode() + "\"\n");
        config.AddInputSection("wacom", "Pad",
                               "Option \"Type\"        \"pad\"\n"
                               "Option \"Device\"      \"" + dev->GetDeviceNode() + "\"\n");
        config.AddInputSection("wacom", "Cursor",
                               "Option \"Type\"        \"cursor\"\n"
                               "Option \"Device\"      \"" + dev->GetDeviceNode() + "\"\n");
        config.AddInputSection("wacom", "Eraser",
                               "Option \"Type\"        \"eraser\"\n"
                               "Option \"Device\"      \"" + dev->GetDeviceNode() + "\"\n");
        config.WriteConfig("/tmp/wacom-hovering-test.conf");
    }
};

TEST_F(WacomHoveringTest, DevicePresent)
{
    SCOPED_TRACE("Test presence of tools as defined in the xorg.conf");

    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(Display(), XIAllDevices, &ndevices);

    bool stylus = false;
    bool pad    = false;
    bool cursor = false;
    bool eraser = false;

    while(ndevices--) {
        if (strcmp(info[ndevices].name, "Stylus") == 0) {
            ASSERT_EQ(stylus, false) << "Duplicate stylus found" << std::endl;
            stylus = true;
        }
        if (strcmp(info[ndevices].name, "Pad") == 0) {
            ASSERT_EQ(pad, false) << "Duplicate pad found" << std::endl;
            pad = true;
        }
        if (strcmp(info[ndevices].name, "Cursor") == 0) {
            ASSERT_EQ(cursor, false) << "Duplicate cursor found" << std::endl;
            cursor = true;
        }
        if (strcmp(info[ndevices].name, "Eraser") == 0) {
            ASSERT_EQ(eraser, false) << "Duplicate eraser found" << std::endl;
            eraser = true;
        }
    }

    ASSERT_EQ(stylus, true);
    ASSERT_EQ(pad,    true);
    ASSERT_EQ(cursor, true);
    ASSERT_EQ(eraser, true);

    XIFreeDeviceInfo(info);
}

TEST_F(WacomHoveringTest, HoveringTest)
{
    SCOPED_TRACE("Test that hovering the Expresskeys pad while\n"
                 "using the stylus does not generate spurious\n"
                 "motion events to (0,0)\n"
                 "https://bugs.freedesktop.org/show_bug.cgi?id=54250");

    XEvent ev;
    Window win;

    /* the server takes a while to start up but the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());

    // Create a fullscreen window
    win = XCreateSimpleWindow (Display(),
                               DefaultRootWindow(Display()),
                               0, 0,
                               WidthOfScreen (DefaultScreenOfDisplay (Display())),
                               HeightOfScreen (DefaultScreenOfDisplay (Display())),
                               0, 0, 0);
    XSelectInput(Display(), win, ButtonPressMask|
                                 ButtonReleaseMask|
                                 PointerMotionMask|
                                 ButtonMotionMask|
                                 KeymapStateMask|
                                 LeaveWindowMask|
                                 ExposureMask|
                                 StructureNotifyMask );
    XMapWindow (Display(), win);
    XSync(Display(), False);

    bool mapped = False;
    while (XPending(Display())) {
        XNextEvent(Display(), &ev);
        if (ev.type == MapNotify)
            mapped = True;
    }
    EXPECT_TRUE (mapped);

    // Play the recorded events
    dev->Play(RECORDINGS_DIR "tablets/Wacom-Intuos5-touch-M-Pen-hovering-expresskeys.events");

    XSync (Display(), False);
    EXPECT_NE(XPending(Display()), 0) << "No event received??" << std::endl;

    // Capture events and check for offending events caused by hovering the Expresskeys
    while(XPending(Display())) {
        XNextEvent(Display(), &ev);
        // Looking for spurious events caused by hovering the Expresskeys
        EXPECT_NE (ev.type,        KeymapNotify);
        EXPECT_NE (ev.type,        LeaveNotify);
        EXPECT_NE (ev.xany.window, None);
    }
}
