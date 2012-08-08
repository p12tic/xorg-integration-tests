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
    virtual void SetUpConfigAndLog(const std::string& prefix);

    /**
     * Set up an event listener that listens for test results. If any result
     * fails, ::failed is set to true.
     */
    void SetUpEventListener();

    virtual void StartServer();
    virtual void SetUp();
    virtual void TearDown();

    int xi2_opcode;
    std::string config_file;
    std::string log_file;
    xorg::testing::XServer server;
    bool failed;

    XOrgConfig config;

private:
    /**
     * Sets ::failed to true if this test failed.
     */
    void OnTestPartResult(const ::testing::TestPartResult &test_part_result);
};

#endif
