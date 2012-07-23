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

        WriteConfig();
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

bool check_for_tool (const char *tool_name, XDeviceInfo *list, int ndevices)
{
    XDeviceInfo *info;
    int loop;
    bool found = false;

    info = list;
    for (loop = 0; loop < ndevices; loop++, info++) {
        if (strcmp(info->name, tool_name) == 0) {
                if (found)
                    return false; // Duplicate
                found = true;
        }
    }
    return found;
}

TEST_P(WacomDriverTest, DeviceNames)
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
    char tool_name[255];
        
    if (tablet.stylus) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.stylus);
        found = check_for_tool (tool_name, list, ndevices);
        ASSERT_EQ(found, true) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;
    }
        
    if (tablet.eraser) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.eraser);
        found = check_for_tool (tool_name, list, ndevices);
        ASSERT_EQ(found, true) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;
    }
        
    if (tablet.cursor) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.cursor);
        found = check_for_tool (tool_name, list, ndevices);
        ASSERT_EQ(found, true) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;
    }
        
    if (tablet.pad) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.pad);
        found = check_for_tool (tool_name, list, ndevices);
        ASSERT_EQ(found, true) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;
    }
        
    if (tablet.touch) {
        snprintf (tool_name, sizeof (tool_name), "%s %s", tablet.name, tablet.touch);
        found = check_for_tool (tool_name, list, ndevices);
        ASSERT_EQ(found, true) << "Tool \"" << tool_name << "\" not found or duplicate" << std::endl;
    }

    XFreeDeviceList (list);
}

WacomDeviceType
get_device_type (Display *dpy, XDeviceInfo *dev)
{
    WacomDeviceType ret;
    static Atom stylus, cursor, eraser, pad, touch;
    XDevice *device;

    ret = WACOM_TYPE_INVALID;

    if ((dev->use == IsXPointer) || (dev->use == IsXKeyboard))
        return ret;

    if (!stylus)
        stylus = XInternAtom (dpy, "STYLUS", False);
    if (!eraser)
        eraser = XInternAtom (dpy, "ERASER", False);
    if (!cursor)
        cursor = XInternAtom (dpy, "CURSOR", False);
    if (!pad)
        pad = XInternAtom (dpy, "PAD", False);
    if (!touch)
        touch = XInternAtom (dpy, "TOUCH", False);

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
    if (ret == WACOM_TYPE_INVALID)
        return ret;

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

INSTANTIATE_TEST_CASE_P(, WacomDriverTest,
        ::testing::ValuesIn(tablets));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
