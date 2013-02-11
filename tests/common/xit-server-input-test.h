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
#ifndef _XIT_SERVER_INPUT_TEST_H_
#define _XIT_SERVER_INPUT_TEST_H_

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
class XITServerInputTest : public XITServerTest {
protected:
    XITServerInputTest();

    /**
     * Require and register for the given XI2 extension. Default is 2.0,
     * will ASSERT if the server supports a version smaller than the given
     * one.
     *
     * @param major The major version to register for
     * @param minor The minor version to register for
     * @param major_return The server-supported major version (if non-NULL)
     * @param minor_return The server-supported minor version (if non-NULL)
     */
    virtual void RequireXI2(int major, int minor,
                            int *major_return = NULL, int *minor_return = NULL);

    /**
     * Starts the server and registers for XI2.
     */
    virtual void StartServer();

    /**
     * Opcode for XI2 events
     */
    int xi2_opcode;

    /**
     * Event base for XI events
     */
    int xi_event_base;

    /**
     * Error base for XI events
     */
    int xi_error_base;

    /**
     * Minimum XI2 major version.
     */
    int xi2_major_minimum;

    /**
     * Minimum XI2 major version.
     */
    int xi2_minor_minimum;
};

/**
 * Most basic input driver test. Use for a TEST_P with a GetParam()
 * returning the driver name.
 * Will set up a AAD off X Server with one input device section that points
 * to a CorePointer device as provided.
 */
class SimpleInputDriverTest : public XITServerInputTest,
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
