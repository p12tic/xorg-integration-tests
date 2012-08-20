#if HAVE_CONFIG_H
#include <config.h>
#endif
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

void InputDriverTest::SetUpConfigAndLog(const std::string& param) {
    std::stringstream s;
    s << "/tmp/Xorg-" << param << ".log";
    server.SetOption("-logfile", s.str());

    s.str(std::string());
    s << "/tmp/" << param << ".conf";
    config.SetPath(s.str());

    config.AddDefaultScreenWithDriver();
    config.AddInputSection(param, "--device--", "Option \"CorePointer\" \"on\"\n");
    config.WriteConfig();
    server.SetOption("-config", config.GetPath());
}

void InputDriverTest::SetUpEventListener() {
    failed = false;

    testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
    listeners.Append(this);
}

void InputDriverTest::SetUp() {
    SetUp("");
}

void InputDriverTest::SetUp(const std::string &param) {
    SetUpEventListener();
    SetUpConfigAndLog(param);
    StartServer();
}

void InputDriverTest::TearDown() {
    if (server.Pid() != -1)
        if (!server.Terminate(3000))
            server.Kill(3000);

    if (!Failed()) {
        config.RemoveConfig();
        server.RemoveLogFile();
    }

    testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
    listeners.Release(this);
}

void InputDriverTest::OnTestPartResult(const ::testing::TestPartResult &test_part_result) {
    failed = test_part_result.failed();
}

bool InputDriverTest::Failed() {
    return failed;
}
