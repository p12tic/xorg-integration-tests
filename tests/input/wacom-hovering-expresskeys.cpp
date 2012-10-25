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

#define EV_MASK     KeyPressMask | KeyReleaseMask | ButtonPressMask | \
		    ButtonReleaseMask | EnterWindowMask | \
		    LeaveWindowMask | PointerMotionMask | \
		    ButtonMotionMask | KeymapStateMask | \
		    ExposureMask | VisibilityChangeMask | \
		    StructureNotifyMask

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
        InitDefaultLogFiles(server, &config);
        server.SetOption("-retro", "");

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("wacom", "Stylus",
                               "Option \"Type\"        \"stylus\"\n"
                               "Option \"DebugLevel\"  \"3\"\n"
                               "Option \"CommonDBG\"   \"3\"\n"
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
        config.WriteConfig();
    }
};

TEST_F(WacomHoveringTest, HoveringTest)
{
    SCOPED_TRACE("Test that hovering the Expresskeys pad while "
                 "using the stylus does not generate spurious "
                 "motion events to (0,0).\n"
                 "Note: This test is useless since the problem cannot be reproduced via evemu\n"
                 "https://bugs.freedesktop.org/show_bug.cgi?id=54250");

    XEvent ev;
    Window win;
    unsigned long white, black;

    /* the server takes a while to start up but the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());

    white = WhitePixel(Display(), DefaultScreen (Display()));
    black = BlackPixel(Display(), DefaultScreen (Display()));

    // Create a fullscreen window
    win  = XCreateSimpleWindow (Display(),
                                DefaultRootWindow(Display()),
                                20, 20, 600, 600, 10, black, white);

    XSelectInput(Display(), win, EV_MASK);

    XMapWindow (Display(), win);
    XSync(Display(), False);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(), Expose, -1, -1, 1000));

    // Play the recorded events
    dev->Play(RECORDINGS_DIR "tablets/Wacom-Intuos5-touch-M-Pen-hovering-expresskeys.events");

    XSync (Display(), False);
    EXPECT_NE(XPending(Display()), 0) << "No event received??" << std::endl;

    // Capture events and check for spurious motion events to (0,0) caused by hovering the Expresskeys
    while(XPending(Display())) {
        XNextEvent(Display(), &ev);
        if (ev.type == MotionNotify)
            EXPECT_FALSE (ev.xmotion.x_root == 0 && ev.xmotion.y_root == 0);
        else if (ev.type == EnterNotify || ev.type == LeaveNotify)
            EXPECT_FALSE (ev.xcrossing.x_root == 0 && ev.xcrossing.y_root == 0);
    }

    while(XPending(Display()))
        XNextEvent(Display(), &ev);
}
