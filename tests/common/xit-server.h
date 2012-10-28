#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _XIT_SERVER_H
#define _XIT_SERVER_H

#include <xorg/gtest/xorg-gtest.h>

#include "xorg-conf.h"

/**
 * Wrapper around the xorg-gtest XServer class that provides default setup
 * routines for the XIT suite
 */
class XITServer : public xorg::testing::XServer {
public:
    XITServer();
    void Start();
    void Start(XOrgConfig &config);

    /**
     * @return a string for the default log file to be used. This log path
     * contains the test name.
     */
    static std::string GetDefaultLogFile();

    /**
     * @return a string for the default config file to be used. This config path
     * contains the test name.
     */
    static std::string GetDefaultConfigFile();

protected:
    /**
     * Return a string of the current test name that's suitable for file names,
     * i.e. has all slashes in the test name replaced with a period.
     *
     * @return Test case name in the form of TestCase.TestName.2 (the .2 part is
     * only present for parameterized tests.
     */
    static std::string GetNormalizedTestName();

};

#endif
