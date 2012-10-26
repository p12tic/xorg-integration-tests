#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _XIT_SERVER_TEST_H_
#define _XIT_SERVER_TEST_H_

#include <xorg/gtest/xorg-gtest.h>

#include "xorg-conf.h"

/**
 * Root class for XIT that require an X server to be started.
 *
 * Handles log/config file creation, automatic path names for those files,
 * etc.
 */
class XITServerTest : public xorg::testing::Test,
                      private ::testing::EmptyTestEventListener {
protected:
    /**
     * Set up an event listener that listens for test results. If any result
     * fails, Failed() will return true.
     */
    void SetUpEventListener();

    /**
     * Set up the config and log file to default file names. These file
     * names are based on the test case and test name.
     *
     * This function is called from SetUp(), before the server is started.
     * Tests that need a different configuration but the same general
     * behavior can override SetUpConfigAndLog() only and leave the rest of
     * the behaviour intact.
     */
    virtual void SetUpConfigAndLog();

    /**
     * Default googletest entry point for setup-work during test fixtures.
     * This implementation simply calls SetUp() with an empty string.
     */
    virtual void SetUp();

    /**
     * Default googletest entry point for clean-up work after test
     * fixtures were run.
     */
    virtual void TearDown();

    /**
     * Starts the server and waits for connections. Once completed,
     * xorg::testing::Test::Display() will be set to the server's display.
     */
    virtual void StartServer();

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

#endif
