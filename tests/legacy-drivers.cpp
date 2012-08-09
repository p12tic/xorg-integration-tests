#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"
#include "helpers.h"

class LegacyInputDriverTest : public InputDriverTest {
public:
    virtual void ConfigureInputDevice(const std::string &driver) {
        config.AddInputSection(driver, "--device--", "Option \"CorePointer\" \"on\"\n");
    }

    virtual void SetUpConfigAndLog(const std::string& param) {
        std::stringstream s;
        s << "/tmp/Xorg-" << param << ".log";
        server.SetOption("-logfile",s.str());

        s.str(std::string());
        s << "/tmp/" << param << ".conf";
        config.SetPath(s.str());

        config.AddDefaultScreenWithDriver();
        ConfigureInputDevice(param);
        config.WriteConfig(config.GetPath());
        server.SetOption("-config", config.GetPath());
    }
};


static int count_devices(Display *dpy) {
    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(dpy, XIAllDevices, &ndevices);
    XIFreeDeviceInfo(info);
    return ndevices;
}

TEST_P(LegacyInputDriverTest, SimpleDeviceSection)
{
    std::string param;

    param = GetParam();

    int expected_devices;

    /* apparently when the acecad driver is loaded neither mouse nor kbd
     * loads - probably a bug but that's current behaviour on RHEL 6.3 */
    if (param.compare("acecad") == 0)
        expected_devices = 4;
    else {
        expected_devices = 6;
	/* xserver git 1357cd7251 , no <default pointer> */
	if (server.GetVersion().compare("1.11.0") > 0)
		expected_devices--;
    }

    ASSERT_EQ(count_devices(Display()), expected_devices);

    /* No joke, they all fail. some fail for missing Device option, others
     * for not finding the device, etc. */
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--"), 0);
}

INSTANTIATE_TEST_CASE_P(, LegacyInputDriverTest,
        ::testing::Values("acecad", "aiptek", "elographics",
                          "fpit", "hyperpen",  "mutouch",
                          "penmount"));

/***********************************************************************
 *                                                                     *
 *                               ACECAD                                *
 *                                                                     *
 ***********************************************************************/
TEST(AcecadInputDriver, WithOptionDevice)
{
    XOrgConfig config;
    xorg::testing::XServer server;

    config.AddInputSection("acecad", "--device--",
                           "Option \"CorePointer\" \"on\"\n"
                           "Option \"Device\" \"/dev/input/event0\"\n");
    config.AddDefaultScreenWithDriver();
    StartServer("acecad-type-stylus", server, config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(dpy, XIAllDevices, &ndevices);

    /* VCP, VCK, xtest, mouse, keyboard, acecad */
    int expected_devices = 7;

    /* xserver git 1357cd7251 , no <default pointer> */
    if (server.GetVersion().compare("1.11.0") > 0)
	    expected_devices--;

    ASSERT_EQ(count_devices(dpy), expected_devices);
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--"), 1);

    config.RemoveConfig();
    server.RemoveLogFile();
}

/***********************************************************************
 *                                                                     *
 *                               AIPTEK                                *
 *                                                                     *
 ***********************************************************************/

TEST(AiptekInputDriver, TypeStylus)
{
    XOrgConfig config;
    xorg::testing::XServer server;

    config.AddInputSection("aiptek", "--device--",
                           "Option \"CorePointer\" \"on\"\n"
                           "Option \"Device\" \"/dev/input/event0\"\n"
                           "Option \"Type\" \"stylus\"");
    config.AddDefaultScreenWithDriver();
    StartServer("aiptek-type-stylus", server, config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    /* VCP, VCK, xtest, mouse, keyboard, aiptek */
    int expected_devices = 7;

    /* xserver git 1357cd7251, no <default pointer> */
    if (server.GetVersion().compare("1.11.0") > 0)
	    expected_devices--;

    ASSERT_EQ(count_devices(dpy), expected_devices);
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--"), 1);

    config.RemoveConfig();
    server.RemoveLogFile();
}

TEST(AiptekInputDriver, TypeCursor)
{
    XOrgConfig config;
    xorg::testing::XServer server;

    config.AddInputSection("aiptek", "--device--",
                           "Option \"CorePointer\" \"on\"\n"
                           "Option \"Device\" \"/dev/input/event0\"\n"
                           "Option \"Type\" \"cursor\"");
    config.AddDefaultScreenWithDriver();
    StartServer("aiptek-type-cursor", server, config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    /* VCP, VCK, xtest, mouse, keyboard, aiptek */
    int expected_devices = 7;

    /* xserver git 1357cd7251, no <default pointer> */
    if (server.GetVersion().compare("1.11.0") > 0)
	    expected_devices--;

    ASSERT_EQ(count_devices(dpy), expected_devices);
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--"), 1);

    config.RemoveConfig();
}

TEST(AiptekInputDriver, TypeEraser)
{
    XOrgConfig config;
    xorg::testing::XServer server;

    config.AddInputSection("aiptek", "--device--",
                           "Option \"CorePointer\" \"on\"\n"
                           "Option \"Device\" \"/dev/input/event0\"\n"
                           "Option \"Type\" \"eraser\"");
    config.AddDefaultScreenWithDriver();
    StartServer("aiptek-type-eraser", server, config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    /* VCP, VCK, xtest, mouse, keyboard, aiptek */
    int expected_devices = 7;

    /* xserver git 1357cd7251, no <default pointer> */
    if (server.GetVersion().compare("1.11.0") > 0)
	    expected_devices--;

    ASSERT_EQ(count_devices(dpy), expected_devices);
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--"), 1);

    config.RemoveConfig();
    server.RemoveLogFile();
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
