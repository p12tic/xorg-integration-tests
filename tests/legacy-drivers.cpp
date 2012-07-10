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
    virtual void ConfigureInputDevice(std::string &driver) {
        config.AddInputSection(driver, "--device--", "Option \"CorePointer\" \"on\"\n");
    }

    virtual void SetUpConfigAndLog() {
        std::string param = GetParam();

        std::stringstream s;
        s << "/tmp/Xorg-" << GetParam() << ".log";
        log_file = s.str();
        server.SetOption("-logfile",log_file);

        s.str(std::string());
        s << "/tmp/" << GetParam() << ".conf";
        config_file = s.str();

        ConfigureInputDevice(param);
        config.WriteConfig(config_file);
        server.SetOption("-config", config_file);
    }
};

TEST_P(LegacyInputDriverTest, SimpleDeviceSection)
{
    std::string param;
    int ndevices;
    XIDeviceInfo *info;

    param = GetParam();
    info = XIQueryDevice(Display(), XIAllDevices, &ndevices);

    int expected_devices;

    /* apparently when the acecad driver is loaded neither mouse nor kbd
     * loads - probably a bug but that's current behaviour on RHEL 6.3 */
    if (param.compare("acecad") == 0)
        expected_devices = 4;
    else
        expected_devices = 6;

    ASSERT_EQ(ndevices, expected_devices);

    bool found = false;
    while(ndevices--) {
        if (strcmp(info[ndevices].name, "--device--") == 0) {
            ASSERT_EQ(found, false) << "Duplicate device" << std::endl;
            found = true;
        }
    }

    /* No joke, they all fail. some fail for missing Device option, others
     * for not finding the device, etc. */
    ASSERT_EQ(found, false);

    XIFreeDeviceInfo(info);
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
    StartServer("acecad-type-stylus", server, config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(dpy, XIAllDevices, &ndevices);

    KillServer(server, config);

    /* VCP, VCK, xtest, mouse, keyboard, acecad */
    ASSERT_EQ(ndevices, 7);
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
    StartServer("aiptek-type-stylus", server, config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(dpy, XIAllDevices, &ndevices);

    KillServer(server, config);

    /* VCP, VCK, xtest, mouse, keyboard, aiptek */
    ASSERT_EQ(ndevices, 7);
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

TEST(AiptekInputDriver, TypeCursor)
{
    XOrgConfig config;
    xorg::testing::XServer server;

    config.AddInputSection("aiptek", "--device--",
                           "Option \"CorePointer\" \"on\"\n"
                           "Option \"Device\" \"/dev/input/event0\"\n"
                           "Option \"Type\" \"cursor\"");
    StartServer("aiptek-type-cursor", server, config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(dpy, XIAllDevices, &ndevices);

    KillServer(server, config);

    /* VCP, VCK, xtest, mouse, keyboard, aiptek */
    ASSERT_EQ(ndevices, 7);
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

TEST(AiptekInputDriver, TypeEraser)
{
    XOrgConfig config;
    xorg::testing::XServer server;

    config.AddInputSection("aiptek", "--device--",
                           "Option \"CorePointer\" \"on\"\n"
                           "Option \"Device\" \"/dev/input/event0\"\n"
                           "Option \"Type\" \"eraser\"");
    StartServer("aiptek-type-eraser", server, config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(dpy, XIAllDevices, &ndevices);
    KillServer(server, config);

    /* VCP, VCK, xtest, mouse, keyboard, aiptek */
    ASSERT_EQ(ndevices, 7);
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

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
