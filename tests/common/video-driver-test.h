#if HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef _VIDEO_DRIVER_TEST_H_
#define _VIDEO_DRIVER_TEST_H_

#include <xorg/gtest/xorg-gtest.h>
#include "xorg-conf.h"

/**
 * A test fixture for testing Video drivers. This class automates basic
 * device setup throught the server config file.
 *
 * Do not instanciate this class directly, subclass it from the test case
 * instead. For simple test cases, use SimpleVideoDriverTest.
 */
class VideoDriverTest : public xorg::testing::Test,
                        private ::testing::EmptyTestEventListener {
protected:
    /**
     * Set up the config and log file. The param is used for both the
     * config file ("param.conf") and the log file ("Xorg-param.log").
     * The default behavior of this function is to set up an xorg.conf with
     * a single input device, using the param as the driver name.
     *
     * This function is called from SetUp(), before the server is started.
     * Tests that need a different configuration but the same general
     * behavior can override SetUpConfigAndLog() only and leave the rest of
     * the behaviour intact.
     *
     * @param param The param used as prefix for the config file and the log file and as input driver.
     */
    virtual void SetUpConfigAndLog(const std::string& param = "");

    /**
     * Set up an event listener that listens for test results. If any result
     * fails, Failed() will return true.
     */
    void SetUpEventListener();

    /**
     * Starts the server and waits for connections. Once completed,
     * xorg::testing::Test::Display() will be set to the server's display
     * and that display will have registered for XI2.
     */
    virtual void StartServer();

    /**
     * Default googletest entry point for setup-work during test fixtures.
     * This implementation simply calls SetUp() with an empty string.
     */
    virtual void SetUp();

    /**
     * Sets up an event listener to watch for test failures and calls
     * SetUpConfigAndLog() for initalisation of the server config. Finally,
     * starts the server.
     *
     * Most test fixtures will not need any extra other than initializing a
     * device and then calling SetUp().
     */
    virtual void SetUp(const std::string &param);

    /**
     * Default googletest entry point for clean-up work after test
     * fixtures were run.
     */
    virtual void TearDown();

    /**
     * @return false if all tests succeeded, true if one failed
     */
    virtual bool Failed();

    /**
     * The X server instance. This server is started with StartServer() but
     * may be started by child classes directly.
     */
    xorg::testing::XServer server;

    /**
     * The server configuration. SetUpConfigAndLog() by default works on
     * this configuration and the server started by StartServer() will then
     * use this configuration.
     */
    XOrgConfig config;

private:
    bool failed;

    /**
     * Callback for test results. If any test fails, ::Failed() will return
     * false.
     */
    void OnTestPartResult(const ::testing::TestPartResult &test_part_result);
};

/**
 * Most basic Video driver test. Use for a TEST_P with a GetParam()
 * returning the driver name.
 * Will set up a AAD off X Server with one device section
 */
class SimpleVideoDriverTest : public VideoDriverTest,
                              public ::testing::WithParamInterface<std::string> {
public:
    /**
     * Calls InputDriverTest::SetUp() with a parameter value of GetParam().
     */
    virtual void SetUp() {
        VideoDriverTest::SetUp(GetParam());
    }
};

#endif
