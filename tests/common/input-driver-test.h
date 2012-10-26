#if HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef _INPUT_DRIVER_TEST_H_
#define _INPUT_DRIVER_TEST_H_

#include <xorg/gtest/xorg-gtest.h>

#include "xit-server-test.h"
#include "helpers.h"

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
     * Register for the given XI2 extension. Default is 2.0, will ASSERT if
     * the server does not support XI2.
     *
     * @return the minor version number returned by the server.
     */
    virtual int RegisterXI2(int major = 2, int minor = 0);

    /**
     * Starts the server and registers for XI2.
     */
    virtual void StartServer();

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
    virtual void SetUpConfigAndLog() {

        std::string driver = GetParam();

        config.AddDefaultScreenWithDriver();
        config.AddInputSection(driver, "--device--", "Option \"CorePointer\" \"on\"\n");
        config.WriteConfig();
        server.SetOption("-config", config.GetPath());
    }
};

#endif
