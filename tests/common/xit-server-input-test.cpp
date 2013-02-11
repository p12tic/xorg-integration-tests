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
#include <stdexcept>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "xit-server-input-test.h"

XITServerInputTest::XITServerInputTest() : xi2_major_minimum(2), xi2_minor_minimum(0) {
}

void XITServerInputTest::RequireXI2(int major, int minor, int *major_return, int *minor_return)
{
    int event_start;
    int error_start;

    ASSERT_TRUE(XQueryExtension(Display(), "XInputExtension", &xi2_opcode,
                         &event_start, &error_start));

    xi_event_base = event_start;
    xi_error_base = error_start;

    XExtensionVersion *version = XGetExtensionVersion(Display(), INAME);
    ASSERT_TRUE(version->present);
    ASSERT_LE(major * 100 + minor, version->major_version * 100 + version->minor_version);

    if (major_return || minor_return) {
        if (major_return)
            *major_return = version->major_version;
        if (minor_return)
            *minor_return = version->minor_version;
    }

    XFree(version);

    ASSERT_EQ(XIQueryVersion(Display(), &major, &minor), Success);
}

void XITServerInputTest::StartServer() {
    XITServerTest::StartServer();
    RequireXI2(xi2_major_minimum, xi2_minor_minimum);
}
