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

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include <xorg/wacom-properties.h>
#include <xorg/xserver-properties.h>
#include <unistd.h>

#include "input-driver-test.h"
#include "device-interface.h"
#include "helpers.h"

#define NUM_ELEMS_MATRIX 9
// Intuos5 maxX=44704 maxY=27940 maxZ=2047 resX=200000 resY=200000  tilt=enabled
#define MAX_X 44704
#define MAX_Y 27940

typedef struct Matrix {
    float elem[NUM_ELEMS_MATRIX];
} Matrix;

/**
 * Wacom driver test class. This class takes a struct Tablet that defines
 * which device should be initialised.
 */
class WacomMatrixTest : public InputDriverTest,
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
        server.SetOption("-logfile", "/tmp/Xorg-wacom-matrix-test.log");
        server.SetOption("-config", "/tmp/wacom-matrix-test.conf");

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("wacom", "Stylus",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"Type\"        \"stylus\"\n"
                               "Option \"Device\"      \"" + dev->GetDeviceNode() + "\"\n");

        config.WriteConfig("/tmp/wacom-matrix-test.conf");
    }
protected:
    int xrandr_event, xrandr_error;
};

TEST_F(WacomMatrixTest, DevicePresent)
{
    SCOPED_TRACE("Test presence of tools as defined in the xorg.conf");

    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(Display(), XIAllDevices, &ndevices);

    bool found = false;
    while(ndevices--) {
        if (strcmp(info[ndevices].name, "Stylus") == 0) {
            ASSERT_EQ(found, false) << "Duplicate device" << std::endl;
            found = true;
        }
    }
    ASSERT_EQ(found, true);
    XIFreeDeviceInfo(info);
}

bool set_input_matrix (Display *dpy, int deviceid, Matrix *matrix)
{
    Atom prop_float, prop_matrix;

    union {
        unsigned char *c;
        float *f;
    } data;
    int format_return;
    Atom type_return;
    unsigned long nitems;
    unsigned long bytes_after;
    int rc;

    prop_float = XInternAtom(dpy, "FLOAT", False);
    prop_matrix = XInternAtom(dpy, XI_PROP_TRANSFORM, False);

    if (!prop_float)
        return False;

    if (!prop_matrix)
        return False;

    rc = XIGetProperty(dpy, deviceid, prop_matrix, 0, 9, False, prop_float,
                       &type_return, &format_return, &nitems, &bytes_after,
                       &data.c);

    if (rc != Success || prop_float != type_return || format_return != 32 ||
        nitems != NUM_ELEMS_MATRIX || bytes_after != 0)
        return False;

    memcpy(data.f, matrix->elem, sizeof(matrix->elem));

    XIChangeProperty(dpy, deviceid, prop_matrix, prop_float,
                     format_return, PropModeReplace, data.c, nitems);

    XFree(data.c);

    return True;
}

/**
 * compute the input trarnsformation matrix to match the area specified
 * by the given (x,y) width x height.
 */
Bool compute_input_matrix (Display *dpy, int x, int y, int width, int height, Matrix *matrix)
{
    // Reset matrix to no transformation by default
    matrix->elem[0] = 1.0f;
    matrix->elem[1] = 0.0f;
    matrix->elem[2] = 0.0f;
    matrix->elem[3] = 0.0f;
    matrix->elem[4] = 1.0f;
    matrix->elem[5] = 0.0f;
    matrix->elem[6] = 0.0f;
    matrix->elem[7] = 0.0f;
    matrix->elem[8] = 1.0f;

    int screen_x      = 0;
    int screen_y      = 0;
    int screen_width  = WidthOfScreen (DefaultScreenOfDisplay (dpy));
    int screen_height = HeightOfScreen (DefaultScreenOfDisplay (dpy));

    if ((x < screen_x) ||
        (y < screen_y) ||
        (x + width > screen_x + screen_width) ||
        (y + height > screen_y + screen_height))
        return False;

    float x_scale      = (float) x / screen_width;
    float y_scale      = (float) y / screen_height;
    float width_scale  = (float) width / screen_width;
    float height_scale = (float) height / screen_height;

    matrix->elem[0] = width_scale;
    matrix->elem[1] = 0.0f;
    matrix->elem[2] = x_scale;

    matrix->elem[3] = 0.0f;
    matrix->elem[4] = height_scale;
    matrix->elem[5] = y_scale;

    matrix->elem[6] = 0.0f;
    matrix->elem[7] = 0.0f;
    matrix->elem[8] = 1.0f;

    return True;
}

/**
 * Moves the stylus from positon (x, y) by n steps in the
 * direction (code) ABS_X or ABS_Y.
 */
void move_stylus (xorg::testing::evemu::Device *dev,
                 int x, int y, int steps, int code)
{
    int i;

    // Move to device coord (x, y)
    dev->PlayOne(EV_ABS, ABS_X, x, True);
    dev->PlayOne(EV_ABS, ABS_Y, y, True);
    dev->PlayOne(EV_ABS, ABS_DISTANCE, 0, True);
    dev->PlayOne(EV_KEY, BTN_TOOL_PEN, 1, True);

    for (i = 0; i < steps; i++) {
        if (code == ABS_X)
            dev->PlayOne(EV_ABS, ABS_X, x + i, True);
        else
            dev->PlayOne(EV_ABS, ABS_Y, y + i, True);

        dev->PlayOne(EV_ABS, ABS_DISTANCE, i, True);
    }
    dev->PlayOne(EV_KEY, BTN_TOOL_PEN, 0, True);
}

void test_area (Display *dpy, xorg::testing::evemu::Device *dev,
                int deviceid, int x, int y, int width, int height)
{
    XEvent ev;

    Matrix matrix;
    compute_input_matrix (dpy, x, y, width, height, &matrix);
    ASSERT_TRUE (set_input_matrix (dpy, deviceid, &matrix));

    XSync(dpy, False);
    while(XPending(dpy))
        XNextEvent(dpy, &ev);

    // Simulate stylus movement for the entire tablet resolution
    move_stylus (dev, 0, 0, MAX_X, ABS_X);
    move_stylus (dev, MAX_X, 0, MAX_Y, ABS_Y);
    move_stylus (dev, 0, 0, MAX_Y, ABS_Y);
    move_stylus (dev, 0, MAX_Y, MAX_X, ABS_X);

    XSync (dpy, False);
    EXPECT_NE(XPending(dpy), 0) << "No event received??" << std::endl;

    // Capture motion events and check they remain within the mapped area
    XSync (dpy, False);
    while(XCheckMaskEvent (dpy, PointerMotionMask, &ev)) {
        EXPECT_TRUE (ev.xmotion.x_root >= x);
        EXPECT_TRUE (ev.xmotion.x_root < x + width);
        EXPECT_TRUE (ev.xmotion.y_root >= y);
        EXPECT_TRUE (ev.xmotion.y_root < y + height);
    }

    while(XPending(dpy))
        XNextEvent(dpy, &ev);
}

TEST_F(WacomMatrixTest, InputMatrix)
{
    SCOPED_TRACE("Test input transformation matrix");

    /* the server takes a while to start up but the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());

    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));

    EXPECT_TRUE(InitRandRSupport(Display(), &xrandr_event, &xrandr_error));

    int monitor_x, monitor_y, monitor_width, monitor_height;
    int n_monitors = GetNMonitors (Display());

    monitor_x = 0;
    monitor_y = 0;
    monitor_width = WidthOfScreen (DefaultScreenOfDisplay (Display()));
    monitor_height = HeightOfScreen (DefaultScreenOfDisplay (Display()));

    if (n_monitors > 0)
        GetMonitorGeometry (Display(), 0, &monitor_x, &monitor_y, &monitor_width, &monitor_height);

    int deviceid;
    FindInputDeviceByName(Display(), "Stylus", &deviceid);
    ASSERT_NE (deviceid, 0);

    XSelectInput(Display(), DefaultRootWindow(Display()), PointerMotionMask |
                                                          ButtonMotionMask);

    // Check with upper right quarter
    test_area (Display(), dev.get(), deviceid, monitor_x, monitor_y, monitor_width / 2, monitor_height / 2);

    // Check with upper left quarter
    test_area (Display(), dev.get(), deviceid, monitor_x + monitor_width / 2, monitor_y, monitor_width / 2, monitor_height / 2);

    // Check with bottom right quarter
    test_area (Display(), dev.get(), deviceid, monitor_x, monitor_y + monitor_height / 2, monitor_width / 2, monitor_height / 2);

    // Check with bottom left quarter
    test_area (Display(), dev.get(), deviceid, monitor_x + monitor_width / 2, monitor_y + monitor_height / 2, monitor_width / 2, monitor_height / 2);
}
