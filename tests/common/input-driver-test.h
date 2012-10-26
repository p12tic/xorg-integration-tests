#if HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef _INPUT_DRIVER_TEST_H_
#define _INPUT_DRIVER_TEST_H_

#include <xorg/gtest/xorg-gtest.h>

#include "xit-server-test.h"

/**
 * A test fixture for testing input drivers. This class automates basic
 * device setup throught the server config file.
 *
 * Do not instanciate this class directly, subclass it from the test case
 * instead. For simple test cases, use SimpleInputDriverTest.
 */
class InputDriverTest : public XITServerTest {
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
    virtual void SetUpConfigAndLog(const std::string& param);

    /**
     * Register for the given XI2 extension. Default is 2.0, will ASSERT if
     * the server does not support XI2.
     *
     * @return the minor version number returned by the server.
     */
    virtual int RegisterXI2(int major = 2, int minor = 0);

    /**
     * Default googletest entry point for setup-work during test fixtures.
     * This implementation simply calls SetUp() with an empty string.
     */
    virtual void SetUp();

    /**
     * Starts the server and registers for XI2.
     */
    virtual void StartServer();

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
     * Opcode for XI2 events
     */
    int xi2_opcode;
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
    /**
     * Calls InputDriverTest::SetUp() with a parameter value of GetParam().
     */
    virtual void SetUp() {
        InputDriverTest::SetUp(GetParam());
    }
};

#endif
