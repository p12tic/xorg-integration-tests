#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XInput.h>

#include "input-driver-test.h"
#include "device-interface.h"
#include "helpers.h"


/**
 * Evdev driver test for keyboard devices. Takes a string as parameter,
 * which is later used for the XkbLayout option.
 */
class PointerGrabTest : public InputDriverTest,
                        public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        InputDriverTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    virtual void SetUpConfigAndLog() {
        InitDefaultLogFiles(server, &config);

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "kbd-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }


    virtual int RegisterXI2(int major, int minor)
    {
        return InputDriverTest::RegisterXI2(2, 1);
    }

};

TEST_F(PointerGrabTest, ImplicitGrabRawEvents)
{
    SCOPED_TRACE("\n"
                 "TESTCASE: this test registers for XI_ButtonPress on a \n"
                 "window and expects raw events to arrive while the \n"
                 "implicit grab is active.");

    ASSERT_GE(RegisterXI2(2, 1), 1) << "This test requires XI 2.1+";

    ::Display *dpy = Display();

    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
                                     DisplayWidth(dpy, DefaultScreen(dpy)),
                                     DisplayHeight(dpy, DefaultScreen(dpy)),
                                     0, 0,
                                     WhitePixel(dpy, DefaultScreen(dpy)));
    XSelectInput(dpy, win, ExposureMask);
    XMapWindow(dpy, win);
    XFlush(dpy);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           Expose,
                                                           -1, -1, 1000));

    XIEventMask mask;

    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_RawMotion);
    mask.mask = new unsigned char[mask.mask_len];
    memset(mask.mask, 0, sizeof(mask.mask));
    XISetMask(mask.mask, XI_ButtonPress);
    XISelectEvents(dpy, win, &mask, 1);

    memset(mask.mask, 0, sizeof(mask.mask));
    XISetMask(mask.mask, XI_RawButtonPress);
    XISetMask(mask.mask, XI_RawMotion);
    XISelectEvents(dpy, DefaultRootWindow(dpy), &mask, 1);

    delete mask.mask;

    XSync(dpy, False);

    /* Move pointer, make sure we receive raw events */
    dev->PlayOne(EV_REL, REL_X, -1, true);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_RawMotion,
                                                           1000));
    XEvent ev;
    XNextEvent(dpy, &ev);
    ASSERT_FALSE(XPending(dpy));

    /* Now press button, make sure sure we still get raw events */
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, true);

    /* raw button events come before normal button events */
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_RawButtonPress,
                                                           1000));
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_ButtonPress,
                                                           1000));
    dev->PlayOne(EV_REL, REL_X, -1, true);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_RawMotion,
                                                           1000));
    XNextEvent(dpy, &ev);
    ASSERT_FALSE(XPending(dpy));
}

TEST_F(PointerGrabTest, GrabDisabledDevices)
{
    SCOPED_TRACE("Grabbing a disabled device must not segfault the server.\n"
                 "https://bugs.freedesktop.org/show_bug.cgi?id=54934");

    Atom prop = XInternAtom(Display(), "Device Enabled", True);
    ASSERT_NE(prop, (Atom)None);

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    unsigned char data = 0;
    XIChangeProperty(Display(), deviceid, prop, XA_INTEGER, 8,
            PropModeReplace, &data, 1);
    XFlush(Display());

    XDevice *xdev = XOpenDevice(Display(), deviceid);
    ASSERT_TRUE(xdev != NULL);

    XGrabDevice(Display(), xdev, DefaultRootWindow(Display()), True,
            0, NULL, GrabModeAsync, GrabModeAsync, CurrentTime);
    XSync(Display(), False);
    ASSERT_EQ(server.GetState(), xorg::testing::Process::RUNNING);
}

#if HAVE_XI22
class TouchGrabTest : public InputDriverTest,
                      public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device.
     */
    virtual void SetUp() {
        SetDevice("tablets/N-Trig-MultiTouch.desc");
        InputDriverTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    virtual void SetUpConfigAndLog() {
        InitDefaultLogFiles(server, &config);
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "kbd-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }


    virtual int RegisterXI2(int major, int minor)
    {
        return InputDriverTest::RegisterXI2(2, 2);
    }

};

class TouchGrabTestMultipleModes : public TouchGrabTest,
                                   public ::testing::WithParamInterface<int>
{
};

TEST_P(TouchGrabTestMultipleModes, ActiveAndPassiveGrab)
{
    int mode = GetParam();
    std::string strmode = (mode == XIAcceptTouch) ? "XIAcceptTouch" : "XIRejectTouch";
    SCOPED_TRACE("\nTESTCASE: Passive and active grab on the same device\n"
                 "must receive an equal number of TouchBegin/TouchEnd "
                 "events\nfor mode" + strmode  + ".\n"
                 "https://bugs.freedesktop.org/show_bug.cgi?id=55738");

    int deviceid;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1);

    XIEventMask mask;
    mask.deviceid = deviceid;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    XIGrabModifiers modifier = {0};
    modifier.modifiers = XIAnyModifier;

    Window win = XDefaultRootWindow(Display());

    XIGrabButton(Display(), deviceid, 1, win, None, GrabModeAsync, GrabModeAsync, False, &mask, 1, &modifier);
    ASSERT_EQ(XIGrabTouchBegin(Display(), deviceid, win, False, &mask, 1, &modifier), Success);
    XIGrabDevice(Display(), deviceid, win, CurrentTime, None, GrabModeAsync, GrabModeAsync, False, &mask);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    XSync(Display(), False);

    int tbegin_received = 0;
    int tend_received = 0;

    while (XPending(Display())) {
        XEvent ev;
        XNextEvent(Display(), &ev);

        if (ev.xcookie.type == GenericEvent &&
            ev.xcookie.extension == xi2_opcode &&
            XGetEventData(Display(), &ev.xcookie)) {
            XIDeviceEvent *event = reinterpret_cast<XIDeviceEvent*>(ev.xcookie.data);
            switch(event->evtype) {
                case XI_TouchBegin:
                    tbegin_received++;
                    XIAllowTouchEvents(Display(), event->deviceid, event->detail, win, mode);
                    break;
                case XI_TouchEnd:
                    tend_received++;
                    break;
            }
        }

        XFreeEventData(Display(), &ev.xcookie);
        XSync(Display(), False);
    }
    ASSERT_FALSE(xorg::testing::XServer::WaitForEventOfType(Display(),
                GenericEvent,
                xi2_opcode,
                XI_TouchEnd,
                500));
    if (mode == XIAcceptTouch) {
        ASSERT_EQ(tbegin_received, 1);
        ASSERT_EQ(tend_received, 1);
    } else {
        ASSERT_EQ(tbegin_received, 2);
        ASSERT_EQ(tend_received, 2);
    }

    delete[] mask.mask;
}

INSTANTIATE_TEST_CASE_P(, TouchGrabTestMultipleModes, ::testing::Values(XIAcceptTouch, XIRejectTouch));

class TouchGrabTestMultipleTaps : public TouchGrabTest,
                                  public ::testing::WithParamInterface<int> { };

TEST_P(TouchGrabTestMultipleTaps, PassiveGrabPointerEmulationMultipleTouchesFastSuccession)
{
    SCOPED_TRACE("\nTESTCASE: a client selecting for core events on the \n"
                 "root window must not receive ButtonPress and Release events \n"
                 "if a passive button grabbing client has GrabModeAsync.\n"
                 "This test exceeds num_touches on the device.\n");

    ::Display *dpy1 = Display();

    XIEventMask mask;
    mask.deviceid = XIAllDevices;
    mask.mask_len = XIMaskLen(XI_ButtonRelease);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_ButtonPress);

    XIGrabModifiers mods;
    mods.modifiers = XIAnyModifier;
    XIGrabButton(dpy1, 2 /* VCP */, 1, DefaultRootWindow(dpy1),
                 None, GrabModeAsync, GrabModeAsync, False, &mask, 1, &mods);
    delete[] mask.mask;

    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy2);
    Window root = DefaultRootWindow(dpy2);

    int major = 2, minor = 0;
    ASSERT_EQ(XIQueryVersion(dpy2, &major, &minor), Success);

    XSelectInput(dpy2, root, PointerMotionMask | ButtonPressMask | ButtonReleaseMask);
    XSync(dpy2, False);
    ASSERT_FALSE(XPending(dpy2));

    int repeats = GetParam();

    /* for this test to succeed, the server mustn't start processing until
       the last touch event physically ends, so no usleep here*/
    for (int i = 0; i < repeats; i++) {
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");
    }

    XSync(dpy1, False);
    XSync(dpy2, False);

    ASSERT_TRUE(XPending(dpy1));
    ASSERT_EQ(XPending(dpy1), repeats); // ButtonPress event
    while (XPending(dpy1)) {
        XEvent ev;
        XNextEvent(dpy1, &ev);
        ASSERT_EQ(ev.type, GenericEvent);
        ASSERT_EQ(ev.xcookie.extension, xi2_opcode);
        ASSERT_EQ(ev.xcookie.evtype, XI_ButtonPress);
    }

    while (XPending(dpy2)) {
        XEvent ev;
        XNextEvent(dpy2, &ev);
        ASSERT_EQ(ev.type, MotionNotify);
    }
}

TEST_P(TouchGrabTestMultipleTaps, PassiveGrabPointerRelease)
{
    SCOPED_TRACE("\nTESTCASE: if a client has a async passive button grab on the "
                 "root window,\na client with a touch selection on the next window "
                 "down must not get touch events.\n"
                 "This test exceeds num_touches on the device.\n");

    ::Display *dpy1 = Display();

    XIEventMask mask;
    mask.deviceid = XIAllDevices;
    mask.mask_len = XIMaskLen(XI_ButtonRelease);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_ButtonPress);

    XIGrabModifiers mods;
    mods.modifiers = XIAnyModifier;
    XIGrabButton(dpy1, 2 /* VCP */, 1, DefaultRootWindow(dpy1),
                 None, GrabModeAsync, GrabModeAsync, False, &mask, 1, &mods);
    delete[] mask.mask;

    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    ASSERT_TRUE(dpy2);
    Window root = DefaultRootWindow(dpy2);

    int major = 2, minor = 2;
    ASSERT_EQ(XIQueryVersion(dpy2, &major, &minor), Success);

    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_TouchEnd);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_TouchBegin);
    XISetMask(mask.mask, XI_TouchUpdate);
    XISetMask(mask.mask, XI_TouchEnd);

    XISelectEvents(dpy2, root, &mask, 1);
    delete[] mask.mask;

    XSync(dpy2, False);
    ASSERT_FALSE(XPending(dpy2));

    int repeats = GetParam();

    /* for this test to succeed, the server mustn't start processing until
       the last touch event physically ends, so no usleep here*/
    for (int i = 0; i < repeats; i++) {
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");
        dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");
    }

    XSync(dpy1, False);
    XSync(dpy2, False);

    ASSERT_TRUE(XPending(dpy1));
    ASSERT_EQ(XPending(dpy1), repeats); // ButtonPress event
    while (XPending(dpy1)) {
        XEvent ev;
        XNextEvent(dpy1, &ev);
        ASSERT_EQ(ev.type, GenericEvent);
        ASSERT_EQ(ev.xcookie.extension, xi2_opcode);
        ASSERT_EQ(ev.xcookie.evtype, XI_ButtonPress);
    }

    if (XPending(dpy2)) {
        XEvent ev;
        XPeekEvent(dpy2, &ev);
        ASSERT_FALSE(XPending(dpy2)) << "Event type " << ev.type << " (extension " <<
            ev.xcookie.extension << " evtype " << ev.xcookie.evtype << ")";
    }
}

INSTANTIATE_TEST_CASE_P(, TouchGrabTestMultipleTaps, ::testing::Range(1, 11)); /* device num_touches is 5 */

#endif /* HAVE_XI22 */

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
