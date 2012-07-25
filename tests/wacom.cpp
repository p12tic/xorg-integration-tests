#include <stdexcept>
#include <map>
#include <unistd.h>

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

#include "wacom_devs.h"

#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include <xorg/wacom-properties.h>
#include <unistd.h>

/* Taken from gnome-settings-daemon */
typedef enum {
	WACOM_TYPE_INVALID =     0,
        WACOM_TYPE_STYLUS  =     (1 << 1),
        WACOM_TYPE_ERASER  =     (1 << 2),
        WACOM_TYPE_CURSOR  =     (1 << 3),
        WACOM_TYPE_PAD     =     (1 << 4),
        WACOM_TYPE_TOUCH   =     (1 << 5),
} WacomDeviceType;

class WacomDriverTest : public xorg::testing::Test,
                        public ::testing::WithParamInterface<Tablet> {
protected:
    void WriteConfig() {
        std::stringstream s;
        Tablet tablet = GetParam();
        
        s << "/tmp/wacom-test-" << std::string(tablet.test_id) << ".conf";
        config_file = s.str();

        std::ofstream conffile(config_file.c_str());
        conffile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

        conffile << ""
"            Section \"ServerFlags\""
"                Option \"Log\" \"flush\""
"            EndSection"
""
"            Section \"ServerLayout\""
"                Identifier \"Dummy layout\""
"                Screen 0 \"Dummy screen\" 0 0"
"                Option \"AutoAddDevices\" \"on\""
"            EndSection"
""
"            Section \"Screen\""
"                Identifier \"Dummy screen\""
"                Device \"Dummy video device\""
"            EndSection"
""
"            Section \"Device\""
"                Identifier \"Dummy video device\""
"                Driver \"dummy\""
"            EndSection";
        server.SetOption("-config", config_file);
    }

    void StartServer() {
        server.SetOption("-logfile",log_file);
        server.Start();
        server.WaitForConnections();
        xorg::testing::Test::SetDisplayString(server.GetDisplayString());

        ASSERT_NO_FATAL_FAILURE(xorg::testing::Test::SetUp());
    }

    virtual void SetUp()
    {
        Tablet tablet = GetParam();
        dev = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device( RECORDINGS_DIR "tablets/" + std::string (tablet.descfile)));

        std::stringstream s;
        s << "/tmp/Xorg-wacom-test-" << std::string(tablet.test_id) << ".log";
        log_file = s.str();

        WriteConfig();/* Taken from gnome-settings-daemon */
        StartServer();
    }

    virtual void TearDown()
    {
        if (server.Pid() != -1)
            if (!server.Terminate())
                server.Kill();
        // Keep logs and config for investigation
        // if (config_file.size())
        //     unlink(config_file.c_str());
        // 
        // if (log_file.size())
        //    unlink(log_file.c_str());
    }

    std::string config_file;
    std::string log_file;
    xorg::testing::XServer server;

    std::auto_ptr<xorg::testing::evemu::Device> dev;
};

void wait_devices (Display *dpy, int time)
{
    // FIXME
    // We rely on hal/udev to add the devices, so this may take some time
    // Not exactly sure how to do that right, we could wi for XI2 events
    // but we can't tell whether or not more device will be added...
    // So for now, take the lamest approach to the problem, sleep...
    sleep (5);
    XSync (dpy, 0);
}

XIDeviceInfo* get_device_info_for_tool (const char *tool_name, XIDeviceInfo *list, int ndevices)
{
    XIDeviceInfo *info, *found;
    int loop;

    found = NULL;
    info = list;

    for (loop = 0; loop < ndevices; loop++, info++) {
        if (strcmp(info->name, tool_name) == 0) {
                if (found)
                    return (XIDeviceInfo *) NULL; // Duplicate
                found = info;
        }
    }

    return found;
}

/* Return True if the given device has the property, or False otherwise */
bool test_property(Display *dpy, int deviceid, const char *prop_name)
{
    Status status;
    Atom prop;
    Atom realtype;
    int realformat;
    unsigned long nitems, bytes_after;
    unsigned char *data;

    prop = XInternAtom (dpy, prop_name, False);
    status = XIGetProperty(dpy, deviceid, prop , 0, 1000, False,
                           AnyPropertyType, &realtype, &realformat,
                           &nitems, &bytes_after, &data);
    if (data != NULL)
        XFree (data);
    
    if (status == Success)
        return True;
        
    return False;
}

TEST_P(WacomDriverTest, DeviceNames)
{
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));

    int ndevices;
    XIDeviceInfo *info, *list, *found;
    bool prop_found;
    Tablet tablet = GetParam();

    /* Wait for devices to settle */
    wait_devices (Display(), 5);
    
    list = XIQueryDevice(Display(), XIAllDevices, &ndevices);
    char tool_name[255];
        
    if (tablet.stylus) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.stylus);
        found = get_device_info_for_tool (tool_name, list, ndevices);
        ASSERT_NE(found, (XIDeviceInfo *) NULL) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;

        prop_found = test_property (Display(), found->deviceid, WACOM_PROP_PRESSURECURVE);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_PRESSURECURVE << " not found on " << tool_name << std::endl;

        prop_found = test_property (Display(), found->deviceid, WACOM_PROP_ROTATION);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_ROTATION << " not found on " << tool_name << std::endl;

        prop_found = test_property (Display(), found->deviceid, WACOM_PROP_TOOL_TYPE);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_TOOL_TYPE << " not found on " << tool_name << std::endl;
    }
        
    if (tablet.eraser) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.eraser);
        found = get_device_info_for_tool (tool_name, list, ndevices);
        ASSERT_NE(found, (XIDeviceInfo *) NULL) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;

        prop_found = test_property (Display(), found->deviceid, WACOM_PROP_PRESSURECURVE);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_PRESSURECURVE << " not found on " << tool_name << std::endl;

        prop_found = test_property (Display(), found->deviceid, WACOM_PROP_ROTATION);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_ROTATION << " not found on " << tool_name << std::endl;
    }
        
    if (tablet.cursor) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.cursor);
        found = get_device_info_for_tool (tool_name, list, ndevices);
        ASSERT_NE(found, (XIDeviceInfo *) NULL) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;

        prop_found = test_property (Display(), found->deviceid,  WACOM_PROP_WHEELBUTTONS);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_WHEELBUTTONS << " not found on " << tool_name << std::endl;
    }
        
    if (tablet.pad) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.pad);
        found = get_device_info_for_tool (tool_name, list, ndevices);
        ASSERT_NE(found, (XIDeviceInfo *) NULL) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;

        prop_found = test_property (Display(), found->deviceid,  WACOM_PROP_STRIPBUTTONS);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_STRIPBUTTONS << " not found on " << tool_name << std::endl;
    }
        
    if (tablet.touch) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.touch);
        found = get_device_info_for_tool (tool_name, list, ndevices);
        ASSERT_NE(found, (XIDeviceInfo *) NULL) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;

        prop_found = test_property (Display(), found->deviceid,  WACOM_PROP_TOUCH);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_TOUCH << " not found on " << tool_name << std::endl;

        prop_found = test_property (Display(), found->deviceid,  WACOM_PROP_ENABLE_GESTURE);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_ENABLE_GESTURE << " not found on " << tool_name << std::endl;

        prop_found = test_property (Display(), found->deviceid,  WACOM_PROP_GESTURE_PARAMETERS);
        EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_GESTURE_PARAMETERS << " not found on " << tool_name << std::endl;
    }

    XIFreeDeviceInfo(list);
}

WacomDeviceType get_device_type (Display *dpy, XDeviceInfo *dev)
{
    WacomDeviceType ret;
    Atom stylus, cursor, eraser, pad, touch;
    XDevice *device;

    ret = WACOM_TYPE_INVALID;

    if ((dev->use == IsXPointer) || (dev->use == IsXKeyboard))
        return ret;

    stylus = XInternAtom (dpy, WACOM_PROP_XI_TYPE_STYLUS, False);
    eraser = XInternAtom (dpy, WACOM_PROP_XI_TYPE_ERASER, False);
    cursor = XInternAtom (dpy, WACOM_PROP_XI_TYPE_CURSOR, False);
    pad    = XInternAtom (dpy, WACOM_PROP_XI_TYPE_PAD,    False);
    touch  = XInternAtom (dpy, WACOM_PROP_XI_TYPE_TOUCH,  False);

    if (dev->type == stylus)
        ret = WACOM_TYPE_STYLUS;
    else if (dev->type == eraser)
        ret = WACOM_TYPE_ERASER;
    else if (dev->type == cursor)
        ret = WACOM_TYPE_CURSOR;
    else if (dev->type == pad)
        ret = WACOM_TYPE_PAD;
    else if (dev->type == touch)
        ret = WACOM_TYPE_TOUCH;

    return ret;
}

bool check_for_type (WacomDeviceType type, Display *dpy, XDeviceInfo *list, int ndevices)
{
    XDeviceInfo *info;
    int loop;
    bool found = false;

    info = list;
    for (loop = 0; loop < ndevices; loop++, info++) {
        if (get_device_type (dpy, info) == type) {
                found = true;
                break;
        }
    }
    return found;
}

TEST_P(WacomDriverTest, DeviceType)
{
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));

    int ndevices;

    Tablet tablet = GetParam();
    XDeviceInfo *info, *list;
    int loop;
    bool found;

    /* Wait for devices to settle */
    wait_devices (Display(), 5);
    
    list = XListInputDevices(Display(), &ndevices);
        
    if (tablet.stylus) {
        found = check_for_type (WACOM_TYPE_STYLUS, Display(), list, ndevices);
        ASSERT_EQ(found, true) << "Stylus not found on " << tablet.name << std::endl;
    }

    if (tablet.eraser) {
        found = check_for_type (WACOM_TYPE_ERASER, Display(), list, ndevices);
        ASSERT_EQ(found, true) << "Eraser not found on " << tablet.name << std::endl;
    }

    if (tablet.cursor) {
        found = check_for_type (WACOM_TYPE_CURSOR, Display(), list, ndevices);
        ASSERT_EQ(found, true) << "Cursor not found on " << tablet.name << std::endl;
    }

    if (tablet.pad) {
        found = check_for_type (WACOM_TYPE_PAD, Display(), list, ndevices);
        ASSERT_EQ(found, true) << "Pad not found on " << tablet.name << std::endl;
    }

    if (tablet.touch) {
        found = check_for_type (WACOM_TYPE_TOUCH, Display(), list, ndevices);
        ASSERT_EQ(found, true) << "Touch not found on " << tablet.name << std::endl;
    }

    XFreeDeviceList (list);
}

bool set_rotate(Display *dpy, int deviceid, const char *rotate)
{
    Status status;
    int rotation = 0;
    Atom prop, type;
    int format;
    unsigned char* data;
    unsigned long nitems, bytes_after;

    if (strcasecmp(rotate, "cw") == 0)
        rotation = 1;
    else if (strcasecmp(rotate, "ccw") == 0)
        rotation = 2;
    else if (strcasecmp(rotate, "half") == 0)
        rotation = 3;

    prop = XInternAtom(dpy, WACOM_PROP_ROTATION, True);
    if (!prop)
        return False;

    status = XIGetProperty(dpy, deviceid, prop, 0, 1000, False,
                           AnyPropertyType, &type, &format,
                           &nitems, &bytes_after, &data);

    if (status != Success || nitems == 0 || format != 8)
        return False;

    *data = rotation;
    
    XIChangeProperty(dpy, deviceid, prop, type, format,
                          PropModeReplace, data, nitems);
    XFlush(dpy);

    return True;
}

int relative_motion (Display *display, xorg::testing::evemu::Device *dev, int axis, int value)
{
    Window w1, w2;
    unsigned int mask;
    int root_x1, root_y1, rel_x, rel_y;
    int root_x2, root_y2;
    XEvent ev;
    
    XSync(display, False);
    XQueryPointer (display, DefaultRootWindow(display), &w1, &w2, &root_x1, &root_y2, &rel_x, &rel_y, &mask);

    root_x2 = root_x1;
    root_y2 = root_y1;
    
    dev->PlayOne(EV_REL, axis, value, 1);
    XSync(display, False);

    if (XPending(display) == 0)
        return 0;
    
    while(XCheckMaskEvent (display, PointerMotionMask, &ev)) {
        root_x2 = ev.xmotion.x_root;
        root_y2 = ev.xmotion.y_root;
    }
    printf ("X axis: %d Y axis: %d\n", root_x2 - root_x1, root_y2 - root_y1);
    if (axis == REL_X)
        return (root_x2 - root_x1);

    if (axis == REL_Y)
        return (root_y2 - root_y1);

    while(XPending(display))
        XNextEvent(display, &ev);

    return 0;
}


TEST_P(WacomDriverTest, Rotation)
{
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));

    int ndevices;
    XIDeviceInfo *info, *list, *found;
    bool status;
    Tablet tablet = GetParam();

    /* Wait for devices to settle */
    wait_devices (Display(), 5);

    XSelectInput(Display(), DefaultRootWindow(Display()), PointerMotionMask);
    /* the server takes a while to start up bust the devices may not respond
       to events yet. Add a noop call that just delays everything long
       enough for this test to work */
    XInternAtom(Display(), "foo", True);
    XFlush(Display());
    
    list = XIQueryDevice(Display(), XIAllDevices, &ndevices);
    char tool_name[255];
    int delta;
    
    if (tablet.stylus) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.stylus);
        found = get_device_info_for_tool (tool_name, list, ndevices);
        ASSERT_NE(found, (XIDeviceInfo *) NULL) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;

        // Try wtith no rotation
        status = set_rotate (Display(), found->deviceid, "none");
        ASSERT_EQ(status, True) << "Failed to rotate " << tool_name << std::endl;
        
        delta = relative_motion (Display(), dev.get(), REL_X, 10);
        EXPECT_TRUE(delta > 0) << "Pointer rotation not applied in X " << tool_name << std::endl;
        
        delta = relative_motion (Display(), dev.get(), REL_Y, 10);
        EXPECT_TRUE(delta > 0) << "Pointer rotation not applied in Y " << tool_name << std::endl;
        
        delta = relative_motion (Display(), dev.get(), REL_X, -10);
        EXPECT_TRUE(delta < 0) << "Pointer rotation not applied in X " << tool_name << std::endl;
        
        delta = relative_motion (Display(), dev.get(), REL_Y, -10);
        EXPECT_TRUE(delta < 0) << "Pointer rotation not applied in Y " << tool_name << std::endl;

        // Set opposite rotation
        status = set_rotate (Display(), found->deviceid, "ccw");
        ASSERT_EQ(status, True) << "Failed to rotate " << tool_name << std::endl;
        
        delta = relative_motion (Display(), dev.get(), REL_X, 10);
        EXPECT_TRUE(delta < 0) << "Pointer rotation not applied in X " << tool_name << std::endl;
        
        delta = relative_motion (Display(), dev.get(), REL_Y, 10);
        EXPECT_TRUE(delta < 0) << "Pointer rotation not applied in Y " << tool_name << std::endl;
        
        delta = relative_motion (Display(), dev.get(), REL_X, -10);
        EXPECT_TRUE(delta > 0) << "Pointer rotation not applied in X " << tool_name << std::endl;
        
        delta = relative_motion (Display(), dev.get(), REL_Y, -10);
        EXPECT_TRUE(delta > 0) << "Pointer rotation not applied in Y " << tool_name << std::endl;
    }

    XIFreeDeviceInfo(list);
}

INSTANTIATE_TEST_CASE_P(, WacomDriverTest,
        ::testing::ValuesIn(tablets));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
