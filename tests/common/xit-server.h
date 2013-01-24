/*
 * Copyright © 2012-2013 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

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
