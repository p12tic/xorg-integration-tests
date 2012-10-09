#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"

int InputDriverTest::RegisterXI2(int major, int minor)
{
    int event_start;
    int error_start;

    if (!XQueryExtension(Display(), "XInputExtension", &xi2_opcode,
                         &event_start, &error_start))
        ADD_FAILURE() << "XQueryExtension returned FALSE";

    int major_return = major;
    int minor_return = minor;
    if (XIQueryVersion(Display(), &major_return, &minor_return) != Success)
        ADD_FAILURE() << "XIQueryVersion failed";
    if (major_return != major)
       ADD_FAILURE() << "XIQueryVersion returned invalid major";

    return minor_return;
}

void InputDriverTest::StartServer() {
    server.SetOption("-noreset", "");
    server.Start();
    xorg::testing::Test::SetDisplayString(server.GetDisplayString());

    ASSERT_NO_FATAL_FAILURE(xorg::testing::Test::SetUp());

    RegisterXI2();
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
    if (server.Pid() != -1) {
        if (!server.Terminate(3000))
            server.Kill(3000);
        EXPECT_EQ(server.GetState(), xorg::testing::Process::FINISHED_SUCCESS) << "Unclean server shutdown";
    }

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
