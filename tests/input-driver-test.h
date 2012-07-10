#ifndef _INPUT_DRIVER_TEST_H_
#define _INPUT_DRIVER_TEST_H_

#include <xorg/gtest/xorg-gtest.h>
#include "xorg-conf.h"

/**
 * A test fixture for testing some input drivers
 *
 */
class InputDriverTest : public xorg::testing::Test,
                        public ::testing::EmptyTestEventListener,
                        public ::testing::WithParamInterface<std::string> {
protected:
    /**
     * Sets ::failed to true if this test failed.
     */
    virtual void OnTestPartResult(const ::testing::TestPartResult &test_part_result);

    /**
     * Set up the config and log file. Called from SetUp, before the server
     * is started so for custom configurations, overwrite this method in a
     * subclass.
     */
    virtual void SetUpConfigAndLog();

    virtual void StartServer();
    virtual void SetUp();
    virtual void TearDown();

    int xi2_opcode;
    std::string config_file;
    std::string log_file;
    xorg::testing::XServer server;
    bool failed;

    XOrgConfig config;
};

#endif
