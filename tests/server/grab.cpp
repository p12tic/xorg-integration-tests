#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XInput.h>

#include "xit-event.h"
#include "xit-server-input-test.h"
#include "device-interface.h"
#include "helpers.h"


/**
 * Evdev driver test for keyboard devices. Takes a string as parameter,
 * which is later used for the XkbLayout option.
 */
class PointerGrabTest : public XITServerInputTest,
                        public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        XITServerInputTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
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


    virtual int RegisterXI2(int major, int minor)
    {
        return XITServerInputTest::RegisterXI2(2, 1);
    }

};

TEST_F(PointerGrabTest, ImplicitGrabRawEvents)
{
    XORG_TESTCASE("This test registers for XI_ButtonPress on a \n"
                  "window and expects raw events to arrive while the \n"
                  "implicit grab is active.\n"
                  "https://bugs.freedesktop.org/show_bug.cgi?id=53897");

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
    XORG_TESTCASE("Grabbing a disabled device must not segfault the server.\n"
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
class TouchGrabTest : public XITServerInputTest,
                      public DeviceInterface {
public:
    /**
     * Initializes a standard mouse device.
     */
    virtual void SetUp() {
        SetDevice("tablets/N-Trig-MultiTouch.desc");
        XITServerInputTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
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


    virtual int RegisterXI2(int major, int minor)
    {
        return XITServerInputTest::RegisterXI2(2, 2);
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
    XORG_TESTCASE("Passive and active grab on the same device\n"
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
    XORG_TESTCASE("A client selecting for core events on the \n"
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
    XIGrabButton(dpy1, VIRTUAL_CORE_POINTER_ID, 1, DefaultRootWindow(dpy1),
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
    XORG_TESTCASE("If a client has a async passive button grab on the "
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
    XIGrabButton(dpy1, VIRTUAL_CORE_POINTER_ID, 1, DefaultRootWindow(dpy1),
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

class TouchOwnershipTest : public TouchGrabTest {
public:
    Window CreateWindow(::Display *dpy, Window parent)
    {
        Window win;
        win = XCreateSimpleWindow(dpy, parent, 0, 0,
                                  DisplayWidth(dpy, DefaultScreen(dpy)),
                                  DisplayHeight(dpy, DefaultScreen(dpy)),
                                  0, 0, 0);
        XSelectInput(dpy, win, StructureNotifyMask);
        XMapWindow(dpy, win);
        if (xorg::testing::XServer::WaitForEventOfType(dpy, MapNotify, -1, -1)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
        } else {
            ADD_FAILURE() << "Failed waiting for Exposure";
        }
        XSelectInput(dpy, win, 0);
        XSync(dpy, False);
        return win;
    }


    void GrabTouchOnWindow(::Display *dpy, Window win, bool ownership = false)
    {
        XIEventMask mask;
        mask.deviceid = XIAllMasterDevices;
        mask.mask_len = XIMaskLen(XI_TouchOwnership);
        mask.mask = new unsigned char[mask.mask_len]();

        XISetMask(mask.mask, XI_TouchBegin);
        XISetMask(mask.mask, XI_TouchEnd);
        XISetMask(mask.mask, XI_TouchUpdate);

        if (ownership)
            XISetMask(mask.mask, XI_TouchOwnership);
        XIGrabModifiers mods = {};
        mods.modifiers = XIAnyModifier;
        ASSERT_EQ(Success, XIGrabTouchBegin(dpy, XIAllMasterDevices,
                           win, False, &mask, 1, &mods));

        delete[] mask.mask;
        XSync(dpy, False);
    }

    void GrabDevice(::Display *dpy, int deviceid, Window win, bool ownership = false)
    {
        XIEventMask mask;
        mask.deviceid = deviceid;
        mask.mask_len = XIMaskLen(XI_TouchOwnership);
        mask.mask = new unsigned char[mask.mask_len]();

        XISetMask(mask.mask, XI_TouchBegin);
        XISetMask(mask.mask, XI_TouchEnd);
        XISetMask(mask.mask, XI_TouchUpdate);

        if (ownership)
            XISetMask(mask.mask, XI_TouchOwnership);

        ASSERT_EQ(Success, XIGrabDevice(dpy, deviceid,
                                        win, CurrentTime, None,
                                        GrabModeAsync, GrabModeAsync,
                                        False, &mask));
        delete[] mask.mask;
        XSync(dpy, False);
    }

    void GrabPointer(::Display *dpy, int deviceid, Window win)
    {
        XIEventMask mask;
        mask.deviceid = deviceid;
        mask.mask_len = XIMaskLen(XI_TouchOwnership);
        mask.mask = new unsigned char[mask.mask_len]();

        XISetMask(mask.mask, XI_ButtonPress);
        XISetMask(mask.mask, XI_ButtonRelease);
        XISetMask(mask.mask, XI_Motion);

        ASSERT_EQ(Success, XIGrabDevice(dpy, deviceid,
                                        win, CurrentTime, None,
                                        GrabModeAsync, GrabModeAsync,
                                        False, &mask));
        delete[] mask.mask;
        XSync(dpy, False);
    }

    void SelectTouchOnWindow(::Display *dpy, Window win, bool ownership = false)
    {
        XIEventMask mask;
        mask.deviceid = XIAllMasterDevices;
        mask.mask_len = XIMaskLen(XI_TouchOwnership);
        mask.mask = new unsigned char[mask.mask_len]();

        XISetMask(mask.mask, XI_TouchBegin);
        XISetMask(mask.mask, XI_TouchEnd);
        XISetMask(mask.mask, XI_TouchUpdate);

        if (ownership)
            XISetMask(mask.mask, XI_TouchOwnership);
        XISelectEvents(dpy, win, &mask, 1);

        delete[] mask.mask;
        XSync(dpy, False);
    }

    void AssertNoEventsPending(::Display *dpy)
    {
        XSync(dpy, False);
        /* A has rejected, expect no more TouchEnd */
        if (XPending(dpy)) {
            XEvent ev;
            XPeekEvent(dpy, &ev);
            std::stringstream ss;
            ASSERT_EQ(XPending(dpy), 0) << "Event type " << ev.type << " (extension " <<
                ev.xcookie.extension << " evtype " << ev.xcookie.evtype << ")";
        }
    }

    /**
     * Return a new synchronized client given our default server connection.
     * Client is initialised for XI 2.2
     */
    ::Display* NewClient(void) {
        ::Display *d = XOpenDisplay(server.GetDisplayString().c_str());
        if (!d)
            ADD_FAILURE() << "Failed to open display for new client.\n";
        XSynchronize(d, True);
        int major = 2, minor = 2;
        if (XIQueryVersion(d, &major, &minor) != Success)
            ADD_FAILURE() << "XIQueryVersion failed on new client.\n";
        return d;
    }
};

TEST_F(TouchOwnershipTest, OwnershipAfterRejectTouch)
{
    XORG_TESTCASE("Client 1 registers a touch grab on the root window.\n"
                  "Clients 2-4 register a touch grab with TouchOwnership on respective subwindows.\n"
                  "Client 5 selects for TouchOwnership events on last window.\n"
                  "TouchBegin/End in the window.\n"
                  "Expect a TouchBegin to all clients.\n"
                  "Reject touch one-by-one in all clients\n"
                  "Expect TouchOwnership to traverse down\n");

    const int NCLIENTS = 5;
    ::Display *dpys[NCLIENTS];
    dpys[0] = Display();
    XSynchronize(dpys[0], True);
    for (int i = 1; i < NCLIENTS; i++)
        dpys[i] = NewClient();

    Window win[NCLIENTS];
    win[0] = DefaultRootWindow(dpys[0]);

    /* Client A has a touch grab on the root window */
    GrabTouchOnWindow(dpys[0], DefaultRootWindow(dpys[0]));
    for (int i = 1; i < NCLIENTS - 1; i++) {
        /* other clients selects with ownership*/
        Window w = CreateWindow(dpys[i], win[i-1]);
        GrabTouchOnWindow(dpys[i], w, true);
        win[i] = w;
    }

    /* last client has selection only */
    Window w = CreateWindow(dpys[NCLIENTS - 1], win[NCLIENTS-2]);
    win[NCLIENTS-1] = w;
    SelectTouchOnWindow(dpys[NCLIENTS-1], w, true);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    int touchid = -1;

    for (int i = 0; i < NCLIENTS; i++) {
        /* Expect touch begin on all clients */
        XITEvent<XIDeviceEvent> tbegin(dpys[i], GenericEvent, xi2_opcode, XI_TouchBegin);
        ASSERT_TRUE(tbegin.ev);
        if (touchid == -1)
            touchid = tbegin.ev->detail;
        else
            ASSERT_EQ(touchid, tbegin.ev->detail);

        /* A has no ownership mask, everyone else doesn't have ownership
         * yet, so everyone only has the TouchBegin so far*/
        AssertNoEventsPending(dpys[i]);
    }

    /* Now reject one-by-one */
    for (int i = 1; i < NCLIENTS; i++) {
        int current_owner = i-1;
        int next_owner = i;
        XIAllowTouchEvents(dpys[current_owner], VIRTUAL_CORE_POINTER_ID, touchid, win[current_owner], XIRejectTouch);

        XITEvent<XIDeviceEvent> tend(dpys[current_owner], GenericEvent, xi2_opcode, XI_TouchEnd);
        ASSERT_TRUE(tend.ev);
        AssertNoEventsPending(dpys[current_owner]);

        /* Now we expect ownership */
        ASSERT_EQ(XPending(dpys[next_owner]), 1);
        XITEvent<XITouchOwnershipEvent> oe(dpys[next_owner], GenericEvent, xi2_opcode, XI_TouchOwnership);
        ASSERT_TRUE(oe.ev);
        ASSERT_EQ(oe.ev->touchid, (unsigned int)touchid);
    }

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    int last_owner = NCLIENTS-1;
    XITEvent<XIDeviceEvent> tend(dpys[last_owner], GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(tend.ev);
    ASSERT_EQ(tend.ev->detail, touchid);

    for (int i = 0; i < NCLIENTS; i++)
        AssertNoEventsPending(dpys[i]);
}

TEST_F(TouchOwnershipTest, NoOwnershipAfterAcceptTouch)
{
    XORG_TESTCASE("Create a window W.\n"
                  "Client A registers a touch grab on the root window.\n"
                  "Client B register for TouchOwnership events on W.\n"
                  "TouchBegin/End in the window.\n"
                  "Expect a TouchBegin to B\n"
                  "Accept touch in A\n"
                  "Expect TouchEnd in B, no ownership\n");
    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    ::Display *dpy2 = NewClient();

    /* Client A has a touch grab on the root window */
    GrabTouchOnWindow(dpy, DefaultRootWindow(dpy));

    /* CLient B selects for events on window */
    Window win = CreateWindow(dpy2, DefaultRootWindow(dpy));
    SelectTouchOnWindow(dpy2, win, true);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    /* Expect touch begin to A */
    XITEvent<XIDeviceEvent> A_begin(dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_TRUE(A_begin.ev);
    int touchid = A_begin.ev->detail;
    int deviceid = A_begin.ev->deviceid;
    Window root = A_begin.ev->root;


    /* Now expect the touch begin to B */
    XITEvent<XIDeviceEvent> B_begin(dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_TRUE(B_begin.ev);
    ASSERT_EQ(B_begin.ev->detail, touchid);

    /* A has not rejected yet, no ownership */
    ASSERT_EQ(XPending(dpy2), 0);

    XIAllowTouchEvents(dpy, deviceid, touchid, root, XIAcceptTouch);

    /* Now we expect TouchEnd on B */
    ASSERT_EQ(XPending(dpy2), 1);
    XITEvent<XIDeviceEvent> B_end(dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(B_end.ev);
    ASSERT_EQ(B_end.ev->detail, touchid);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    XITEvent<XIDeviceEvent> A_end(dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(A_end.ev);
    ASSERT_EQ(A_end.ev->detail, touchid);

    AssertNoEventsPending(dpy2);
}

TEST_F(TouchOwnershipTest, ActiveGrabOwnershipAcceptTouch)
{
    XORG_TESTCASE("Client A actively grabs the device.\n"
                  "Client B register for TouchOwnership events on the root window.\n"
                  "TouchBegin/End in the window.\n"
                  "Expect a TouchBegin to A and B\n"
                  "Accept touch in A\n"
                  "Expect TouchEnd in B, no ownership\n");
    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    ::Display *dpy2 = NewClient();

    /* CLient B selects for events on window */
    Window win = CreateWindow(dpy2, DefaultRootWindow(dpy));
    SelectTouchOnWindow(dpy2, win, true);

    /* Client A grabs the device */
    GrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    /* Expect touch begin to A */
    XITEvent<XIDeviceEvent> A_begin(dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_TRUE(A_begin.ev);
    int touchid = A_begin.ev->detail;
    int deviceid = A_begin.ev->deviceid;
    Window root = A_begin.ev->root;

    /* Now expect the touch begin to B */
    XITEvent<XIDeviceEvent> B_begin(dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_TRUE(B_begin.ev);
    ASSERT_EQ(B_begin.ev->detail, touchid);

    /* A has not rejected yet, no ownership */
    ASSERT_EQ(XPending(dpy2), 0);

    XIAllowTouchEvents(dpy, deviceid, touchid, root, XIAcceptTouch);

    /* Now we expect TouchEnd */
    ASSERT_EQ(XPending(dpy2), 1);
    XITEvent<XIDeviceEvent> B_end(dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(B_end.ev);
    ASSERT_EQ(B_end.ev->detail, touchid);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    XITEvent<XIDeviceEvent> A_end(dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(A_end.ev);
    ASSERT_EQ(A_end.ev->detail, touchid);

    AssertNoEventsPending(dpy2);
}

TEST_F(TouchOwnershipTest, ActiveGrabOwnershipRejectTouch)
{
    XORG_TESTCASE("Client A actively grabs the device.\n"
                  "Client B register for TouchOwnership events on the root window.\n"
                  "TouchBegin/End in the window.\n"
                  "Expect a TouchBegin to A and B\n"
                  "Accept touch in A\n"
                  "Expect TouchEnd in B, no ownership\n");
    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    ::Display *dpy2 = NewClient();

    /* CLient B selects for events on window */
    Window win = CreateWindow(dpy2, DefaultRootWindow(dpy));
    SelectTouchOnWindow(dpy2, win, true);

    /* Client A grabs the device */
    GrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    /* Expect touch begin to A */
    XITEvent<XIDeviceEvent> A_begin(dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_TRUE(A_begin.ev);
    int touchid = A_begin.ev->detail;
    int deviceid = A_begin.ev->deviceid;
    Window root = A_begin.ev->root;

    /* Now expect the touch begin to B */
    XITEvent<XIDeviceEvent> B_begin(dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_TRUE(B_begin.ev);
    ASSERT_EQ(B_begin.ev->detail, touchid);

    /* A has not rejected yet, no ownership */
    ASSERT_EQ(XPending(dpy2), 0);

    XIAllowTouchEvents(dpy, deviceid, touchid, root, XIRejectTouch);

    XITEvent<XIDeviceEvent> A_end(dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(A_end.ev);
    ASSERT_EQ(A_end.ev->detail, touchid);

    /* Now we expect TouchOwnership */
    ASSERT_EQ(XPending(dpy2), 1);

    XITEvent<XITouchOwnershipEvent> oe(dpy2, GenericEvent, xi2_opcode, XI_TouchOwnership);
    ASSERT_TRUE(oe.ev);
    ASSERT_EQ(oe.ev->touchid, (unsigned int)touchid);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    XITEvent<XIDeviceEvent> B_end(dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(B_end.ev);

    AssertNoEventsPending(dpy2);
}

TEST_F(TouchOwnershipTest, ActiveGrabOwnershipUngrabDevice)
{
    XORG_TESTCASE("Client A actively grabs the device.\n"
                  "Client B register for TouchOwnership events on the root window.\n"
                  "TouchBegin/End in the window.\n"
                  "Expect a TouchBegin to A and B\n"
                  "Accept touch in A\n"
                  "Expect TouchEnd in B, no ownership\n");
    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    ::Display *dpy2 = NewClient();

    /* CLient B selects for events on window */
    Window win = CreateWindow(dpy2, DefaultRootWindow(dpy));
    SelectTouchOnWindow(dpy2, win, true);

    /* Client A grabs the device */
    GrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    /* Expect touch begin to A */
    XITEvent<XIDeviceEvent> A_begin(dpy, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_TRUE(A_begin.ev);
    int touchid = A_begin.ev->detail;
    int deviceid = A_begin.ev->deviceid;

    /* Now expect the touch begin to B */
    XITEvent<XIDeviceEvent> B_begin(dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_TRUE(B_begin.ev);
    ASSERT_EQ(B_begin.ev->detail, touchid);

    /* A has not rejected yet, no ownership */
    ASSERT_EQ(XPending(dpy2), 0);

    XIUngrabDevice(dpy, deviceid, CurrentTime);

    /* A needs TouchEnd */
    XITEvent<XIDeviceEvent> A_end(dpy, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(A_end.ev);
    ASSERT_EQ(A_end.ev->detail, touchid);

    /* Now we expect TouchOwnership on B */
    ASSERT_EQ(XPending(dpy2), 1);
    XITEvent<XITouchOwnershipEvent> oe(dpy2, GenericEvent, xi2_opcode, XI_TouchOwnership);
    ASSERT_TRUE(oe.ev);
    ASSERT_EQ(oe.ev->touchid, (unsigned int)touchid);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    XITEvent<XIDeviceEvent> B_end(dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(B_end.ev);
    ASSERT_EQ(B_end.ev->detail, touchid);

    AssertNoEventsPending(dpy);
}

TEST_F(TouchOwnershipTest, ActivePointerGrabForWholeTouch)
{
    XORG_TESTCASE("Client A pointer-grabs the device.\n"
                  "Client B registers for TouchOwnership events on a window.\n"
                  "TouchBegin/End in the window.\n"
                  "Expect ButtonPress/Release to A.\n"
                  "Expect TouchBegin/End to B without ownership.");

    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    ::Display *dpy2 = NewClient();

    /* CLient B selects for events on window */
    Window win = CreateWindow(dpy2, DefaultRootWindow(dpy));
    SelectTouchOnWindow(dpy2, win, true);

    /* Client A grabs the device */
    GrabPointer(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    XITEvent<XIDeviceEvent> A_btnpress(dpy, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_TRUE(A_btnpress.ev);

    XITEvent<XIDeviceEvent> B_begin(dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_TRUE(B_begin.ev);

    /* No ownership event on the wire */
    AssertNoEventsPending(dpy2);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    XITEvent<XIDeviceEvent> A_btnrelease(dpy, GenericEvent, xi2_opcode, XI_ButtonRelease);
    ASSERT_TRUE(A_btnrelease.ev);

    /* One event on the wire and it's TouchEnd, not ownership */
    ASSERT_EQ(XPending(dpy2), 1);

    XIUngrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, CurrentTime);

    XITEvent<XIDeviceEvent> B_end(dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(B_end.ev);

    AssertNoEventsPending(dpy);
}

TEST_F(TouchOwnershipTest, ActivePointerUngrabDuringTouch)
{
    XORG_TESTCASE("Client A pointer-grabs the device.\n"
                  "Client B registers for TouchOwnership events on a window.\n"
                  "TouchBegin in the window.\n"
                  "Expect ButtonPress to A.\n"
                  "Ungrab pointer.\n"
                  "Expect TouchBegin and TouchOwnership to B.\n"
                  "TouchEnd in the window, expect TouchEnd on B.\n");

    ::Display *dpy = Display();
    XSynchronize(dpy, True);

    ::Display *dpy2 = NewClient();

    /* CLient B selects for events on window */
    Window win = CreateWindow(dpy2, DefaultRootWindow(dpy));
    SelectTouchOnWindow(dpy2, win, true);

    /* Client A grabs the device */
    GrabPointer(dpy, VIRTUAL_CORE_POINTER_ID, DefaultRootWindow(dpy));

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_begin.events");

    XITEvent<XIDeviceEvent> A_btnpress(dpy, GenericEvent, xi2_opcode, XI_ButtonPress);
    ASSERT_TRUE(A_btnpress.ev);

    XITEvent<XIDeviceEvent> B_begin(dpy2, GenericEvent, xi2_opcode, XI_TouchBegin);
    ASSERT_TRUE(B_begin.ev);

    /* No ownership event on the wire */
    AssertNoEventsPending(dpy2);

    XIUngrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, CurrentTime);

    XITEvent<XITouchOwnershipEvent> B_ownership(dpy2, GenericEvent, xi2_opcode, XI_TouchOwnership);
    ASSERT_TRUE(B_ownership.ev);

    dev->Play(RECORDINGS_DIR "tablets/N-Trig-MultiTouch.touch_1_end.events");

    XITEvent<XIDeviceEvent> B_end(dpy2, GenericEvent, xi2_opcode, XI_TouchEnd);
    ASSERT_TRUE(B_end.ev);

    AssertNoEventsPending(dpy);
}

#endif /* HAVE_XI22 */

