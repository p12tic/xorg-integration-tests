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
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    virtual void SetUpConfigAndLog(const std::string &param) {
        server.SetOption("-logfile", "/tmp/Xorg-xibuttongrab-test.log");
        server.SetOption("-config", "/tmp/xorg-xibuttongrab-test.conf");

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
        config.WriteConfig("/tmp/xorg-xibuttongrab-test.conf");
    }
};

static int
grab_buttons (Display *dpy, Window win, int deviceid, Bool owner)
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
                           owner,
                           &evmask,
                           1,
                           &mods);
    XFlush(dpy);

    return status;
}

static int
ungrab_buttons (Display *dpy, Window win, int deviceid)
{
    XIGrabModifiers mods;
    int status;

    mods.modifiers = XIAnyModifier;

    status = XIUngrabButton (dpy,
                             deviceid,
                             XIAnyButton,
                             win,
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

    ::Display *dpy = Display();

    XSetWindowAttributes attr;

    attr.background_pixel  = BlackPixel (dpy, DefaultScreen (dpy));
    attr.override_redirect = 1;
    attr.event_mask = ButtonPressMask|
                      ButtonReleaseMask|
                      ExposureMask;

    Window win = XCreateWindow(dpy,
                               DefaultRootWindow(dpy),
                               0, 0,
                               DisplayWidth(dpy, DefaultScreen(dpy)),
                               DisplayHeight(dpy, DefaultScreen(dpy)),
                               0, 
                               DefaultDepth (dpy, DefaultScreen (dpy)), 
                               InputOutput, 
                               DefaultVisual (dpy, DefaultScreen (dpy)),
                               CWBackPixel|CWOverrideRedirect|CWEventMask,
                               &attr);
    XMapRaised (dpy, win);
    XFlush(dpy);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           Expose,
                                                           -1, -1, 1000));
                                                           
    int deviceid;

    ASSERT_EQ(FindInputDeviceByName(Display(), "pad", &deviceid), 1);

    setup_event_mask (dpy, DefaultRootWindow(dpy), deviceid);
    setup_event_mask (dpy, win, deviceid);

    // First, check with owner_events == False, the root win should get the event
    grab_buttons (dpy, DefaultRootWindow(dpy), deviceid, False);
    XSync(dpy, False);

    dev->PlayOne(EV_KEY, BTN_2, 1, true);
    
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonPress,
                                                           1000));
    XEvent ev;
    XNextEvent(dpy, &ev);

    XGenericEventCookie *cookie = &ev.xcookie;

    ASSERT_TRUE(cookie->type == GenericEvent);
    ASSERT_TRUE(cookie->extension == xi2_opcode);    
    ASSERT_TRUE(XGetEventData(dpy, cookie) != 0);

    XIDeviceEvent *xev = (XIDeviceEvent *) cookie->data;
    ASSERT_TRUE(cookie->evtype == XI_ButtonPress);
    ASSERT_TRUE(xev->event == DefaultRootWindow(dpy));

    XFreeEventData(dpy, cookie);
    ASSERT_FALSE(XPending(dpy));

    ASSERT_FALSE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                            ButtonPressMask,
                                                            0,
                                                            ButtonPress,
                                                            1000));

    ungrab_buttons (dpy, DefaultRootWindow(dpy), deviceid);

    // Second, check with owner_events == True, the top win should get the event
    ungrab_buttons (dpy, DefaultRootWindow(dpy), deviceid);
    grab_buttons (dpy, DefaultRootWindow(dpy), deviceid, True);
    XSync(dpy, False);

    dev->PlayOne(EV_KEY, BTN_1, 1, true);
    
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonPress,
                                                           1000));
    XNextEvent(dpy, &ev);
    cookie = &ev.xcookie;

    ASSERT_TRUE(cookie->type == GenericEvent);
    ASSERT_TRUE(cookie->extension == xi2_opcode);    
    ASSERT_TRUE(XGetEventData(dpy, cookie) != 0);

    xev = (XIDeviceEvent *) cookie->data;
    ASSERT_TRUE(cookie->evtype == XI_ButtonPress);
    ASSERT_TRUE(xev->event == win);

    XFreeEventData(dpy, cookie);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
