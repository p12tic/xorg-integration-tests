#if HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef _VIDEO_DRIVER_TEST_H_
#define _VIDEO_DRIVER_TEST_H_

#include <xorg/gtest/xorg-gtest.h>

#include "xit-server-test.h"
#include "helpers.h"

/**
 * A test fixture for testing Video drivers. This class automates basic
 * device setup throught the server config file.
 *
 * Do not instanciate this class directly, subclass it from the test case
 * instead. For simple test cases, use SimpleVideoDriverTest.
 */
class VideoDriverTest : public XITServerTest {
    /* implementation is obsolete with XITServerTest */
};

/**
 * Most basic Video driver test. Use for a TEST_P with a GetParam()
 * returning the driver name.
 * Will set up a AAD off X Server with one device section
 */
class SimpleVideoDriverTest : public VideoDriverTest,
                              public ::testing::WithParamInterface<std::string> {
public:
    virtual void SetUpConfigAndLog() {
        InitDefaultLogFiles(server, &config);

        std::string driver = GetParam();

        config.AddDefaultScreenWithDriver(driver, driver);
        config.WriteConfig();
    }
};

#endif
