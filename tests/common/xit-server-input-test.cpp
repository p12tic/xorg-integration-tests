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

int XITServerInputTest::RegisterXI2(int major, int minor)
{
    int event_start;
    int error_start;

    if (!XQueryExtension(Display(), "XInputExtension", &xi2_opcode,
                         &event_start, &error_start))
        ADD_FAILURE() << "XQueryExtension returned FALSE";
    else {
        xi_event_base = event_start;
        xi_error_base = error_start;
    }

    int major_return = major;
    int minor_return = minor;
    if (XIQueryVersion(Display(), &major_return, &minor_return) != Success)
        ADD_FAILURE() << "XIQueryVersion failed";
    if (major_return != major)
       ADD_FAILURE() << "XIQueryVersion returned invalid major";

    return minor_return;
}

void XITServerInputTest::StartServer() {
    XITServerTest::StartServer();
    RegisterXI2();
}
