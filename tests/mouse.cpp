#include <stdexcept>
#include <map>
#include <fstream>
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
 * Mouse driver test.
 */
class MouseDriverTest : public InputDriverTest {
public:
    /**
     * Initializes a standard mouse device with two wheels.
     */
    virtual void SetUp() {
        dev = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc"
                    )
                );
        InputDriverTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single mouse CorePointer device based on
     * the evemu device.
     */
    virtual void SetUpConfigAndLog(const std::string &param) {
        server.SetOption("-logfile", "/tmp/Xorg-mouse-driver.log");
        server.SetOption("-config", "/tmp/mouse-driver.conf");

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("mouse", "--device--",
                               "Option \"ZAxisMapping\" \"4 5 6 7\"\n"
                               "Option \"Protocol\" \"ImPS/2\"\n"
                               "Option \"CorePointer\" \"on\"\n");
        config.WriteConfig("/tmp/mouse-driver.conf");
    }

protected:
    /**
     * The evemu device to generate events.
     */
    std::auto_ptr<xorg::testing::evemu::Device> dev;
};

void scroll_wheel_event(::Display *display,
                        xorg::testing::evemu::Device *dev,
                        int axis, int value, unsigned int button) {

    dev->PlayOne(EV_REL, axis, value, 1);
    XSync(display, False);

    ASSERT_EQ(xorg::testing::XServer::WaitForEventOfType(display, ButtonPress, -1, -1, 1000), true);

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

    ASSERT_EQ(nevents, abs(value));
}


TEST_F(MouseDriverTest, ScrollWheel)
{
    XSelectInput(Display(), DefaultRootWindow(Display()), ButtonPressMask | ButtonReleaseMask);
    XFlush(Display());
    XSync(Display(), False);

    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, 1, 4);
    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, 2, 4);
    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, 3, 4);

    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, -1, 5);
    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, -2, 5);
    scroll_wheel_event(Display(), dev.get(), REL_WHEEL, -3, 5);

    /* Vertical scrolling only, it appears we can't send HScroll events */
}

TEST_F(MouseDriverTest, DevNode)
{
    Atom node_prop = XInternAtom(Display(), "Device Node", True);

    ASSERT_NE(node_prop, (Atom)None) << "This requires server 1.11";

    int deviceid = -1;
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--", &deviceid), 1) << "Failed to find device.";

    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    char *data;
    XIGetProperty(Display(), deviceid, node_prop, 0, 100, False,
                  AnyPropertyType, &type, &format, &nitems, &bytes_after,
                  (unsigned char**)&data);
    ASSERT_EQ(type, XA_STRING);

    std::string devnode(data);
    ASSERT_EQ(devnode.compare("/dev/input/mice"), 0);

    XFree(data);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
