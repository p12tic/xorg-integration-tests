#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include "video-driver-test.h"
#include "helpers.h"

void VideoDriverTest::StartServer() {
    server.Start();
    xorg::testing::Test::SetDisplayString(server.GetDisplayString());

    ASSERT_NO_FATAL_FAILURE(xorg::testing::Test::SetUp());
}

void VideoDriverTest::SetUpConfigAndLog(const std::string& param) {
    InitDefaultLogFiles(server, &config);

    config.AddDefaultScreenWithDriver(param, param);
    config.WriteConfig();
}

void VideoDriverTest::SetUp() {
    SetUp("");
}

void VideoDriverTest::SetUp(const std::string &param) {
    SetUpEventListener();
    SetUpConfigAndLog(param);
    StartServer();
}

