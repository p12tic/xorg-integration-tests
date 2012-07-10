#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"

void InputDriverTest::WriteConfig(const char *param) {
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

void InputDriverTest::AddInputSection(std::string driver,
                                      std::string identifier,
                                      std::string options) {
        std::ofstream conffile(config_file.c_str(), std::ios_base::app);
        conffile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

        conffile << ""
"            Section \"InputDevice\"\n"
"                Identifier \"" << identifier << "\"\n"
"                Driver \"" << driver << "\"\n"
                 << options <<
"            EndSection\n";
        server.SetOption("-config", config_file);
}

void InputDriverTest::StartServer() {
        server.SetOption("-logfile",log_file);
        server.Start();
        server.WaitForConnections();
        xorg::testing::Test::SetDisplayString(server.GetDisplayString());

        ASSERT_NO_FATAL_FAILURE(xorg::testing::Test::SetUp());

        int event_start;
        int error_start;

        ASSERT_TRUE(XQueryExtension(Display(), "XInputExtension", &xi2_opcode,
                                    &event_start, &error_start));

        int major = 2;
        int minor = 0;

        ASSERT_EQ(Success, XIQueryVersion(Display(), &major, &minor));
    }

void InputDriverTest::SetUp() {
        failed = false;

        testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
        listeners.Append(this);

        std::stringstream s;
        s << "/tmp/Xorg-" << GetParam() << ".log";
        log_file = s.str();

        WriteConfig(GetParam());
        AddInputSection(GetParam());
        StartServer();
    }

void InputDriverTest::TearDown() {
        if (server.Pid() != -1)
            if (!server.Terminate())
                server.Kill();

        if (!failed) {
            if (config_file.size())
                unlink(config_file.c_str());

            if (log_file.size())
                unlink(log_file.c_str());
        }

        testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
        listeners.Release(this);
}

void InputDriverTest::OnTestPartResult(const ::testing::TestPartResult &test_part_result) {
    failed = test_part_result.failed();
}
