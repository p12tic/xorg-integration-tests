#ifndef _INPUT_DRIVER_TEST_H_
#define _INPUT_DRIVER_TEST_H_

#include <xorg/gtest/xorg-gtest.h>
#include "xorg-conf.h"

/**
 * A test fixture for testing input drivers. This class automates basic
 * device setup throught the server config file.
 *
 * Do not instanciate this class directly, subclass it from the test case
 * instead. For simple test cases, use SimpleInputDriverTest.
 */
class InputDriverTest : public xorg::testing::Test,
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

    virtual void StartServer();
    virtual void SetUp();
    virtual void SetUp(const std::string &param);
    virtual void TearDown();

    /**
     * @return false if all tests succeeded, true if one failed
     */
    virtual bool Failed();

    int xi2_opcode;
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

/**
 * Most basic input driver test. Use for a TEST_P with a GetParam()
 * returning the driver name.
 * Will set up a AAD off X Server with one input device section that points
 * to a CorePointer device as provided.
 */
class SimpleInputDriverTest : public InputDriverTest,
                              public ::testing::WithParamInterface<std::string> {
public:
    virtual void SetUp() {
        InputDriverTest::SetUp(GetParam());
    }
};

#endif
