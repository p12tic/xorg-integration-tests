#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _DEVICE_INTERFACE_H_
#define _DEVICE_INTERFACE_H_

#include <xorg/gtest/xorg-gtest.h>

/**
 * A test fixture for testing input drivers. This class automates basic
 * device setup throught the server config file.
 *
 * Do not instanciate this class directly, subclass it from the test case
 * instead. For simple test cases, use SimpleInputDriverTest.
 */
class DeviceInterface {
protected:
    /**
     * The evemu device to generate events.
     */
    std::auto_ptr<xorg::testing::evemu::Device> dev;

    virtual void SetDevice(const std::string& path, const std::string &basedir = RECORDINGS_DIR);
};

#endif
