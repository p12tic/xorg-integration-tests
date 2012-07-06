#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

/**
 * A test fixture for testing some input drivers
 *
 */
class InputDriverTest : public xorg::testing::Test,
                        public ::testing::WithParamInterface<const char*> {
protected:
    void WriteConfig(const char *param) {
        std::stringstream s;
        s << "/tmp/" << param << ".conf";
        config_file = s.str();

        std::ofstream conffile(config_file.c_str());
        conffile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

        std::string driver(param);

        conffile << ""
"            Section \"ServerLayout\""
"                Identifier \"Dummy layout\""
"                Screen 0 \"Dummy screen\" 0 0"
"                Option \"AutoAddDevices\" \"off\""
"                InputDevice \"--device--\""
""
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

    void AddDriverSection(const char *param, const char *options = "") {
        std::ofstream conffile(config_file.c_str(), std::ios_base::app);
        conffile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

        std::string driver(param);

        conffile << ""
"            Section \"InputDevice\""
"                Identifier \"--device--\""
"                Driver \"" << driver << "\""
                 << options <<
"            EndSection";
        server.SetOption("-config", config_file);
    }

    void StartServer() {
        server.Start();
        server.WaitForConnections();
        xorg::testing::Test::SetDisplayString(server.GetDisplayString());

        ASSERT_NO_FATAL_FAILURE(xorg::testing::Test::SetUp());

        int event_start;
        int error_start;

        ASSERT_TRUE(XQueryExtension(Display(), "XInputExtension", &xi2_opcode_,
                                    &event_start, &error_start));

        int major = 2;
        int minor = 0;

        ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));
    }

    virtual void SetUp()
    {
        WriteConfig(GetParam());
        AddDriverSection(GetParam());
        StartServer();
    }

    virtual void TearDown()
    {
        if (server.Pid() != -1)
            if (!server.Terminate())
                server.Kill();

        if (config_file.size())
            unlink(config_file.c_str());
    }

    int xi2_opcode_;
    std::string config_file;
    xorg::testing::XServer server;
};

TEST_P(InputDriverTest, DriverDevice)
{
    const char *param;
    int ndevices;
    XIDeviceInfo *info;

    param = GetParam();
    info = XIQueryDevice(Display(), XIAllDevices, &ndevices);
    /* VCP, VCK, 2 test devices, forced mouse/keyboard */
    ASSERT_EQ(ndevices, 6) << "Drivers required for this test: mouse, "
                               "keyboard, " << param;

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

INSTANTIATE_TEST_CASE_P(, InputDriverTest, ::testing::Values("acecad"));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
