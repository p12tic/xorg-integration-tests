#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"

void InputDriverTest::StartServer() {
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

void InputDriverTest::SetUpConfigAndLog(const std::string& prefix) {
    std::stringstream s;
    s << "/tmp/Xorg-" << prefix << ".log";
    log_file = s.str();
    server.SetOption("-logfile",log_file);

    s.str(std::string());
    s << "/tmp/" << prefix << ".conf";
    config_file = s.str();

    config.AddDefaultScreenWithDriver();
    config.AddInputSection(prefix);
    config.WriteConfig(config_file);
    server.SetOption("-config", config_file);
}

void InputDriverTest::SetUp() {
    failed = false;

    testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
    listeners.Append(this);

    SetUpConfigAndLog(GetParam());

    StartServer();
}

void InputDriverTest::TearDown() {
    if (server.Pid() != -1)
        if (!server.Terminate(3000))
            server.Kill(3000);

    if (!failed) {
        config.RemoveConfig();
        server.RemoveLogFile();
    }

    testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
    listeners.Release(this);
}

void InputDriverTest::OnTestPartResult(const ::testing::TestPartResult &test_part_result) {
    failed = test_part_result.failed();
}
