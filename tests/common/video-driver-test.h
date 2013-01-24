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

        std::string driver = GetParam();

        config.AddDefaultScreenWithDriver(driver, driver);
        config.WriteConfig();
    }
};

#endif
