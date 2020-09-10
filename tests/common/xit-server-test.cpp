/*
 * Copyright © 2012-2013 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>

#include "xit-server-test.h"
#include "helpers.h"

#define TEST_TIMEOUT 10

void XITServerTest::SetUpEventListener() {
    failed = false;
    synchronized = false;

    testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
    listeners.Append(this);
}

void XITServerTest::SetUpConfigAndLog() {
}

void XITServerTest::SetUp() {
    SetUpEventListener();
    SetUpConfigAndLog();
    StartServer();
}

::Display* XITServerTest::Display() {
    ::Display *dpy = xorg::testing::Test::Display();
    if (!synchronized) {
        XSynchronize(dpy, True);
        synchronized = true;
    }
    return dpy;
}

bool XITServerTest::WaitForDevice(const std::string &name, int timeout)
{
    int result;
    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    if (!dpy) {
        ADD_FAILURE() << "Failed to open separate Display.\n";
        return false;
    }

    XSynchronize(dpy, True);
    result = xorg::testing::XServer::WaitForDevice(dpy, name);
    XCloseDisplay(dpy);

    return result;
}

void XITServerTest::TearDown() {
    alarm(0);
    if (server.Pid() != -1) {
        if (!server.Terminate(3000))
            server.Kill(3000);
        EXPECT_EQ(server.GetState(), xorg::testing::Process::FINISHED_SUCCESS) << "Unclean server shutdown";
        failed = failed || (server.GetState() != xorg::testing::Process::FINISHED_SUCCESS);

        std::ifstream logfile(server.GetLogFilePath().c_str());
        std::string line;
        std::string bug_warn("BUG");
        if (logfile.is_open()) {
            while(getline(logfile, line)) {
                size_t found = line.find(bug_warn);
                bool error = (found != std::string::npos);
                EXPECT_FALSE(error) << "BUG warning found in log" << std::endl << line << std::endl;
                failed = failed || error;
                break;
            }
        }
    }

    if (!Failed()) {
        config.RemoveConfig();
        server.RemoveLogFile();
    }

    testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
    listeners.Release(this);
}

class XITServerTimeoutError : public std::runtime_error {
    public:
        XITServerTimeoutError(const std::string &msg) : std::runtime_error(msg) {}
};

static void sighandler_alarm(int sig)
{
    static int exception = 0;

    if (exception++ == 0) {
        alarm(2);
        FAIL() << "Test has timed out (" << __func__ << "). Adjust TEST_TIMEOUT (" << TEST_TIMEOUT << "s) if needed.";
    } else {
        exception = 0;
        throw XITServerTimeoutError("Test has timed out.  Adjust TEST_TIMEOUT if needed");
    }
}

void XITServerTest::StartServer() {
    /* No test takes longer than 60 seconds unless we have some envs set
       that suggest we're actually debugging the server */
    if (!getenv("XORG_GTEST_XSERVER_SIGSTOP") &&
        !getenv("XORG_GTEST_XSERVER_KEEPALIVE") &&
        !getenv("XORG_GTEST_USE_VALGRIND")) {
        alarm(TEST_TIMEOUT);
        signal(SIGALRM, sighandler_alarm);
    }

    server.SetOption("-noreset", "");
    server.SetOption("-logverbose", "12");
    server.Start();

    std::string display;
    const char *dpy = getenv("XORG_GTEST_XSERVER_OVERRIDE_DISPLAY");
    if (dpy)
        display = std::string(dpy);
    else
        display = server.GetDisplayString();

    xorg::testing::Test::SetDisplayString(display);

    ASSERT_NO_FATAL_FAILURE(xorg::testing::Test::SetUp());
}

bool XITServerTest::Failed() {
    return failed;
}

void XITServerTest::OnTestPartResult(const ::testing::TestPartResult &test_part_result) {
    failed = failed || test_part_result.failed();
}

std::string XITServer::GetNormalizedTestName() {
    const ::testing::TestInfo *const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();

    std::string testname = test_info->test_case_name();
    testname += ".";
    testname += test_info->name();

    /* parameterized tests end with /0, replace with '.'*/
    size_t found;
    while ((found = testname.find_first_of("/")) != std::string::npos)
        testname[found] = '.';

    return testname;
}

std::string XITServer::GetDefaultLogFile() {
    return std::string(LOG_BASE_PATH) + std::string("/") + "xit_" + GetNormalizedTestName() + std::string(".log");
}

std::string XITServer::GetDefaultConfigFile() {
    return std::string(LOG_BASE_PATH) + std::string("/") + "xit_" + GetNormalizedTestName() + std::string(".conf");
}
