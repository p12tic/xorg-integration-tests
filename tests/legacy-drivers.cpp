#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"

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

class AcecadInputDriverTest : public LegacyInputDriverTest {
    public:
        virtual void ConfigureInputDevice(std::string &driver) {
            config.AddInputSection(driver, "--device--",
                                   "Option \"CorePointer\" \"on\"\n"
                                   "Option \"Device\" \"/dev/input/event0\"\n");
        }
};

TEST_P(AcecadInputDriverTest, WithOptionDevice)
{
    std::string param;
    int ndevices;
    XIDeviceInfo *info;

    param = GetParam();
    info = XIQueryDevice(Display(), XIAllDevices, &ndevices);

    /* when the acecad driver succeeds (even with bogus device)
     * mouse and keyboard load too */
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

INSTANTIATE_TEST_CASE_P(, AcecadInputDriverTest, ::testing::Values(std::string("acecad")));

/***********************************************************************
 *                                                                     *
 *                               AIPTEK                                *
 *                                                                     *
 ***********************************************************************/

class AiptekInputDriverTest : public LegacyInputDriverTest {
    public:
        virtual void ConfigureInputDevice(std::string &driver) {
            config.AddInputSection(driver, "--device--",
                                   "Option \"CorePointer\" \"on\"\n"
                                   "Option \"Device\" \"/dev/input/event0\"\n"
                                   "Option \"Type\" \"stylus\"");
        }
};

TEST_P(AiptekInputDriverTest, WithOptionDevice)
{
    std::string param;
    int ndevices;
    XIDeviceInfo *info;

    param = GetParam();
    info = XIQueryDevice(Display(), XIAllDevices, &ndevices);

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

INSTANTIATE_TEST_CASE_P(, AiptekInputDriverTest, ::testing::Values(std::string("aiptek")));


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
