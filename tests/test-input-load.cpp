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
"            Section \"ServerFlags\"\n"
"                Option \"Log\" \"flush\"\n"
"            EndSection\n"
"\n"
"            Section \"ServerLayout\"\n"
"                Identifier \"Dummy layout\"\n"
"                Screen 0 \"Dummy screen\" 0 0\n"
"                Option \"AutoAddDevices\" \"off\"\n"
"                InputDevice \"--device--\"\n"
"            EndSection\n"
"\n"
"            Section \"Screen\"\n"
"                Identifier \"Dummy screen\"\n"
"                Device \"Dummy video device\"\n"
"            EndSection\n"
"\n"
"            Section \"Device\"\n"
"                Identifier \"Dummy video device\"\n"
"                Driver \"dummy\"\n"
"            EndSection\n";
        server.SetOption("-config", config_file);
    }

    void AddDriverSection(const char *param, const char *options = "") {
        std::ofstream conffile(config_file.c_str(), std::ios_base::app);
        conffile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

        std::string driver(param);

        conffile << ""
"            Section \"InputDevice\"\n"
"                Identifier \"--device--\"\n"
"                Driver \"" << driver << "\"\n"
                 << options <<
"            EndSection\n";
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
        std::stringstream s;
        s << "/tmp/Xorg-" << GetParam() << ".log";
        log_file = s.str();

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

        if (log_file.size())
            unlink(log_file.c_str());
    }

    std::string config_file;
    std::string log_file;
    xorg::testing::XServer server;
};

TEST_P(InputDriverTest, DriverDevice)
{
    std::ifstream in_file(log_file.c_str());
    std::string line;
    std::string error_msg("Failed to load module");
    std::string param(GetParam());

    // grep for error message in the log file...
    error_msg +=  " \"" + param + "\"";
    if(in_file.is_open())
    {
        while (getline (in_file, line))
        {
            size_t found = line.find(error_msg);
            Bool error = (found != std::string::npos);
            ASSERT_FALSE (error) << "Module " << param << " failed to load" << std::endl << line <<  std::endl;
        }
    }
}

INSTANTIATE_TEST_CASE_P(, InputDriverTest,
        ::testing::Values("acecad", "aiptek", "elographics",
                          "fpit", "hyperpen",  "mutouch",
                          "penmount", "wacom", "synaptics",
                          "keyboard", "mouse", "vmmouse",
                          "evdev"));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
