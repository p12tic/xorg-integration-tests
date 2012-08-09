#ifndef _INPUT_DRIVER_TEST_H_
#define _INPUT_DRIVER_TEST_H_

#include <xorg/gtest/xorg-gtest.h>
#include "xorg-conf.h"

/**
 * A test fixture for testing some input drivers
 *
 */
class InputDriverTest : public xorg::testing::Test,
                        private ::testing::EmptyTestEventListener,
                        public ::testing::WithParamInterface<std::string> {
protected:

    /**
     * Set up the config and log file. Called from SetUp, before the server
     * is started so for custom configurations, overwrite this method in a
     * subclass.
     */
    virtual void SetUpConfigAndLog(const std::string& param);

    /**
     * Set up an event listener that listens for test results. If any result
     * fails, Failed() will return true.
     */
    void SetUpEventListener();

    virtual void StartServer();
    virtual void SetUp();
    virtual void TearDown();

    /**
     * @return false if all tests succeeded, true if one failed
     */
    virtual bool Failed();

    int xi2_opcode;
    std::string log_file;
    xorg::testing::XServer server;

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
