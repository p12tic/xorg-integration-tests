#include <stdexcept>
#include <map>
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

class SynapticsDriverTest : public InputDriverTest {
public:
    virtual void SetUp() {

        dev = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "touchpads/SynPS2-Synaptics-TouchPad.desc"
                    )
                );
        InputDriverTest::SetUp();
    }

    virtual void SetUpConfigAndLog(const std::string &prefix) {
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

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
