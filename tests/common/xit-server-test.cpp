#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <fstream>

#include "xit-server-test.h"
#include "helpers.h"

#define TEST_TIMEOUT 60

void XITServerTest::SetUpEventListener() {
    failed = false;

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

static void sighandler_alarm(int signal)
{
    FAIL() << "Test has timed out (" << __func__ << "). Adjust TEST_TIMEOUT (" << TEST_TIMEOUT << "s) if needed.";
}

void XITServerTest::StartServer() {
    /* No test takes longer than 60 seconds */
    alarm(60);
    signal(SIGALRM, sighandler_alarm);

    server.SetOption("-noreset", "");
    server.Start();
    xorg::testing::Test::SetDisplayString(server.GetDisplayString());

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
    return std::string(LOG_BASE_PATH) + std::string("/") + GetNormalizedTestName() + std::string(".log");
}

std::string XITServer::GetDefaultConfigFile() {
    return std::string(LOG_BASE_PATH) + std::string("/") + GetNormalizedTestName() + std::string(".conf");
}
