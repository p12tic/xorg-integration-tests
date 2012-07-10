#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"

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

        s.str(std::string());
        s << "/tmp/" << GetParam() << ".conf";
        config_file = s.str();

        config.AddInputSection(GetParam());
        config.WriteConfig(config_file);
        server.SetOption("-config", config_file);
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
