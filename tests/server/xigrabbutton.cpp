#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"
#include "device-interface.h"
#include "helpers.h"


/**
 */
class XIGrabButtonTest : public InputDriverTest,
                         public DeviceInterface {
public:
    /**
     * Initializes a wacom pad device.
     */
    virtual void SetUp() {
        SetDevice("tablets/Wacom-Cintiq-21UX2.desc");
        InputDriverTest::SetUp();
    }

    /**
     */
    virtual void SetUpConfigAndLog(const std::string &param) {
        InitDefaultLogFiles(server, &config);

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("wacom", "pad",
                               "    Option \"Type\" \"pad\"\n"
                               "    Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        config.AddInputSection("wacom", "stylus",
                               "    Option \"Type\" \"stylus\"\n"
                               "    Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        config.AddInputSection("wacom", "eraser",
                               "    Option \"Type\" \"eraser\"\n"
                               "    Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "kbd-device",
                               "    Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }
};

static int
grab_buttons (Display *dpy, Window win, int deviceid)
{
    XIGrabModifiers mods;
    unsigned char mask[1] = { 0 };
    XIEventMask evmask;
    int status;

    evmask.deviceid = deviceid;
    evmask.mask_len = sizeof(mask);
    evmask.mask = mask;

    XISetMask (mask, XI_ButtonRelease);
    XISetMask (mask, XI_ButtonPress);

    mods.modifiers = XIAnyModifier;

    status = XIGrabButton (dpy,
                           deviceid,
                           XIAnyButton,
                           win,
                           None,
                           GrabModeAsync,
                           GrabModeAsync,
                           False,
                           &evmask,
                           1,
                           &mods);
    XFlush(dpy);

    return status;
}

static int
setup_event_mask (Display *dpy, Window win, int deviceid)
{
    unsigned char mask[1] = { 0 };
    XIEventMask evmask;
    int status;

    evmask.deviceid = deviceid;
    evmask.mask_len = sizeof(mask);
    evmask.mask = mask;

    XISetMask (evmask.mask, XI_ButtonPress);
    XISetMask (evmask.mask, XI_ButtonRelease);

    status = XISelectEvents (dpy,
                             win,
                             &evmask,
                             1);
    return status;
}

TEST_F(XIGrabButtonTest, GrabWindowTest)
{
    SCOPED_TRACE("\n"
                 "TESTCASE: this test grabs buttons on the root window\n"
                 "and maps a fullscreen override redirect window and expects\n"
                 "events to arrive in the root window and not the regular window");

    ASSERT_GE(RegisterXI2(), 0) << "This test requires XI2" << std::endl;

    ::Display *dpy1 = XOpenDisplay (server.GetDisplayString().c_str());
    ::Display *dpy2 = XOpenDisplay (server.GetDisplayString().c_str());

    XSetWindowAttributes attr;

    attr.background_pixel  = BlackPixel (dpy2, DefaultScreen (dpy2));
    attr.override_redirect = 1;
    attr.event_mask = ButtonPressMask|
                      ButtonReleaseMask|
                      ExposureMask;

    Window win = XCreateWindow(dpy2,
                               DefaultRootWindow(dpy2),
                               0, 0,
                               DisplayWidth(dpy2, DefaultScreen(dpy2)),
                               DisplayHeight(dpy2, DefaultScreen(dpy2)),
                               0,
                               DefaultDepth (dpy2, DefaultScreen (dpy2)),
                               InputOutput,
                               DefaultVisual (dpy2, DefaultScreen (dpy2)),
                               CWBackPixel|CWOverrideRedirect|CWEventMask,
                               &attr);
    XMapRaised (dpy2, win);
    XFlush(dpy2);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy2,
                                                           Expose,
                                                           -1, -1, 1000));

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(dpy2, "pad", &deviceid), 1);

    // First, check without the grab, the top win should get the event
    XSync(dpy2, False);

    dev->PlayOne(EV_KEY, BTN_1, 1, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy2,
                                                           ButtonPressMask,
                                                           0,
                                                           ButtonPress,
                                                           1000));

    XEvent ev;
    XNextEvent(dpy2, &ev);

    ASSERT_TRUE(ev.type == ButtonPress);
    XButtonEvent *bev = (XButtonEvent *) &ev;
    ASSERT_TRUE(bev->window == win);

    while (XPending(dpy1))
        XNextEvent (dpy1, &ev);
    ASSERT_FALSE(XPending(dpy1));

    while (XPending(dpy2))
        XNextEvent (dpy2, &ev);
    ASSERT_FALSE(XPending(dpy2));

    // Second, check with XIGrabButton on the root win, the root win should get the event
    setup_event_mask (dpy1, DefaultRootWindow(dpy1), deviceid);
    grab_buttons (dpy1, DefaultRootWindow(dpy1), deviceid);
    XSync(dpy1, False);

    dev->PlayOne(EV_KEY, BTN_2, 1, true);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy1,
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonPress,
                                                           1000));
    XNextEvent(dpy1, &ev);

    XGenericEventCookie *cookie = &ev.xcookie;

    ASSERT_TRUE(cookie->type == GenericEvent);
    ASSERT_TRUE(cookie->extension == xi2_opcode);
    ASSERT_TRUE(XGetEventData(dpy1, cookie) != 0);

    XIDeviceEvent *xev = (XIDeviceEvent *) cookie->data;
    ASSERT_TRUE(cookie->evtype == XI_ButtonPress);
    ASSERT_TRUE(xev->event == DefaultRootWindow(dpy1));

    XFreeEventData(dpy1, cookie);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy2,
                                                           ButtonPressMask,
                                                           0,
                                                           ButtonPress,
                                                           1000));

    XNextEvent(dpy2, &ev);
    ASSERT_TRUE(ev.type == ButtonPress);
    bev = (XButtonEvent *) &ev;
    ASSERT_TRUE(bev->window == win);

    XCloseDisplay(dpy1);
    XCloseDisplay(dpy2);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
