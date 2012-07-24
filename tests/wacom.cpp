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

#include "wacom_devs.h"

#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include <xorg/wacom-properties.h>

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
        s << "/tmp/wacom.conf";
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
        s << "/tmp/Xorg-wacom.log";
        log_file = s.str();

        WriteConfig();/* Taken from gnome-settings-daemon */
        StartServer();
    }

    virtual void TearDown()
    {
        if (server.Pid() != -1)
            if (!server.Terminate())
                server.Kill();

        if (config_file.size())
            unlink(config_file.c_str());

        if (log_file.size())
            unlink(log_file.c_str());
    }

    std::string config_file;
    std::string log_file;
    xorg::testing::XServer server;

    std::auto_ptr<xorg::testing::evemu::Device> dev;
};

XDeviceInfo* get_device_info_for_tool (const char *tool_name, XDeviceInfo *list, int ndevices)
{
    XDeviceInfo *info, *found;
    int loop;

    found = NULL;
    info = list;

    for (loop = 0; loop < ndevices; loop++, info++) {
        if (strcmp(info->name, tool_name) == 0) {
                if (found)
                    return (XDeviceInfo *) NULL; // Duplicate
                found = info;
        }
    }

    return found;
}

/* Return True if the given device has the property, or False otherwise */
bool test_property(Display *dpy, XDevice* dev, const char *prop_name)
{
    int nprops_return;
    Atom *properties;
    Atom prop;
    bool found = False;

    prop = XInternAtom (dpy, prop_name, False);
    properties = XListDeviceProperties(dpy, dev, &nprops_return);
    while(nprops_return--) {
	if (properties[nprops_return] == prop) {
            found = True;
            break;
        }
    }

    XFree(properties);
    return found;
}

TEST_P(WacomDriverTest, DeviceNames)
{
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));

    int ndevices;
    XDeviceInfo *info, *list, *found;
    XDevice *device;
    bool prop_found;
    Tablet tablet = GetParam();

    list = XListInputDevices(Display(), &ndevices);
    char tool_name[255];
        
    if (tablet.stylus) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.stylus);
        found = get_device_info_for_tool (tool_name, list, ndevices);
        EXPECT_NE(found, (XDeviceInfo *) NULL) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;
        if (found) {
            device = XOpenDevice(Display(), found->id);
            EXPECT_NE(device, (XDevice *) NULL) << "Cannot open device for " << tool_name << std::endl;;

            prop_found = test_property (Display(), device, WACOM_PROP_PRESSURECURVE);
            EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_PRESSURECURVE << " not found on " << tool_name << std::endl;

            prop_found = test_property (Display(), device, WACOM_PROP_ROTATION);
            EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_ROTATION << " not found on " << tool_name << std::endl;

            prop_found = test_property (Display(), device, WACOM_PROP_TOOL_TYPE);
            EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_TOOL_TYPE << " not found on " << tool_name << std::endl;

            XCloseDevice (Display(), device);
        }
    }
        
    if (tablet.eraser) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.eraser);
        found = get_device_info_for_tool (tool_name, list, ndevices);
        EXPECT_NE(found, (XDeviceInfo *) NULL) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;
        if (found) {
            device = XOpenDevice(Display(), found->id);
            EXPECT_NE(device, (XDevice *) NULL) << "Cannot open device for " << tool_name << std::endl;;

            prop_found = test_property (Display(), device, WACOM_PROP_PRESSURECURVE);
            EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_PRESSURECURVE << " not found on " << tool_name << std::endl;

            prop_found = test_property (Display(), device, WACOM_PROP_ROTATION);
            EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_ROTATION << " not found on " << tool_name << std::endl;

            XCloseDevice (Display(), device);
        }
    }
        
    if (tablet.cursor) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.cursor);
        found = get_device_info_for_tool (tool_name, list, ndevices);
        EXPECT_NE(found, (XDeviceInfo *) NULL) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;
        if (found) {
            device = XOpenDevice(Display(), found->id);
            EXPECT_NE(device, (XDevice *) NULL) << "Cannot open device for " << tool_name << std::endl;;

            prop_found = test_property (Display(), device,  WACOM_PROP_WHEELBUTTONS);
            EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_WHEELBUTTONS << " not found on " << tool_name << std::endl;

            XCloseDevice (Display(), device);
        }
    }
        
    if (tablet.pad) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.pad);
        found = get_device_info_for_tool (tool_name, list, ndevices);
        EXPECT_NE(found, (XDeviceInfo *) NULL) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;
        if (found) {
            device = XOpenDevice(Display(), found->id);
            EXPECT_NE(device, (XDevice *) NULL) << "Cannot open device for " << tool_name << std::endl;;

            prop_found = test_property (Display(), device,  WACOM_PROP_STRIPBUTTONS);
            EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_STRIPBUTTONS << " not found on " << tool_name << std::endl;

            XCloseDevice (Display(), device);
        }
    }
        
    if (tablet.touch) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.touch);
        found = get_device_info_for_tool (tool_name, list, ndevices);
        EXPECT_NE(found, (XDeviceInfo *) NULL) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;
        if (found) {
            device = XOpenDevice(Display(), found->id);
            EXPECT_NE(device, (XDevice *) NULL) << "Cannot open device for " << tool_name << std::endl;

            prop_found = test_property (Display(), device,  WACOM_PROP_TOUCH);
            EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_TOUCH << " not found on " << tool_name << std::endl;

            prop_found = test_property (Display(), device,  WACOM_PROP_ENABLE_GESTURE);
            EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_ENABLE_GESTURE << " not found on " << tool_name << std::endl;

            prop_found = test_property (Display(), device,  WACOM_PROP_GESTURE_PARAMETERS);
            EXPECT_EQ(prop_found, True) << "Property " << WACOM_PROP_GESTURE_PARAMETERS << " not found on " << tool_name << std::endl;

            XCloseDevice (Display(), device);
        }
    }

    XFreeDeviceList (list);
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
    list = XListInputDevices(Display(), &ndevices);
    int i = 0;
        
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

bool set_rotate(Display *dpy, XDevice *dev, const char *rotate)
{
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

    XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
                       &type, &format, &nitems, &bytes_after, &data);

    if (nitems == 0 || format != 8)
        return False;

    *data = rotation;
    XChangeDeviceProperty(dpy, dev, prop, type, format,
                          PropModeReplace, data, nitems);
    XFlush(dpy);

    return True;
}

INSTANTIATE_TEST_CASE_P(, WacomDriverTest,
        ::testing::ValuesIn(tablets));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
