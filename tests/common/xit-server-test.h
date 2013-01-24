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

#ifndef _XIT_SERVER_TEST_H_
#define _XIT_SERVER_TEST_H_

#include <xorg/gtest/xorg-gtest.h>

#include "xorg-conf.h"
#include "xit-server.h"

/**
 * Root class for XIT that require an X server to be started.
 *
 * Handles log/config file creation, automatic path names for those files,
 * etc.
 */
class XITServerTest : public xorg::testing::Test,
                      private ::testing::EmptyTestEventListener {
protected:
    /**
     * Set up an event listener that listens for test results. If any result
     * fails, Failed() will return true.
     */
    void SetUpEventListener();

    /**
     * Set up the config and log file to default file names. These file
     * names are based on the test case and test name.
     *
     * This function is called from SetUp(), before the server is started.
     * Tests that need a different configuration but the same general
     * behavior can override SetUpConfigAndLog() only and leave the rest of
     * the behaviour intact.
     */
    virtual void SetUpConfigAndLog();

    /**
     * Default googletest entry point for setup-work during test fixtures.
     * This implementation simply calls SetUp() with an empty string.
     */
    virtual void SetUp();

    /**
     * Default googletest entry point for clean-up work after test
     * fixtures were run.
     */
    virtual void TearDown();

    /**
     * Starts the server and waits for connections. Once completed,
     * xorg::testing::Test::Display() will be set to the server's display.
     */
    virtual void StartServer();

    /**
     * @return false if all tests succeeded, true if one failed
     */
    virtual bool Failed();

    /**
     * When called the first time, the display connection is synchronized
     * with XSynchronize(). This is only done once, a test-case that does
     * not want the connection synchronized can undo so.
     *
     * @return The display connection to our server, synchronized
     */
    virtual ::Display* Display();

    /**
     * The X server instance. This server is started with StartServer() but
     * may be started by child classes directly.
     */
    XITServer server;

    /**
     * The server configuration. SetUpConfigAndLog() by default works on
     * this configuration and the server started by StartServer() will then
     * use this configuration.
     */
    XOrgConfig config;

private:
    bool failed;

    bool synchronized;

    /**
     * Callback for test results. If any test fails, ::Failed() will return
     * false.
     */
    void OnTestPartResult(const ::testing::TestPartResult &test_part_result);
};

#endif
