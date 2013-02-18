/*
 * Copyright © 2013 Red Hat, Inc.
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

#include <fstream>

#include <xorg/gtest/xorg-gtest.h>

#include "xit-server-input-test.h"
#include "device-interface.h"
#include "helpers.h"

using namespace xorg::testing;

class Xephyr24bppTest : public XITServerTest {
    virtual void SetUpConfigAndLog() {
        config.SetAutoAddDevices(true);
        config.AppendRawConfig(
            "Section \"ServerLayout\"\n"
            "	Identifier     \"X.org Configured\"\n"
            "	Screen        \"Screen0\"\n"
            "EndSection\n"
            "\n"
            "Section \"ServerFlags\"\n"
            "   Option \"Pixmap\" \"24\""
            "EndSection\n"
            "\n"
            "Section \"Device\"\n"
            "	Identifier  \"Card0\"\n"
            "	Driver      \"dummy\"\n"
            "EndSection\n"
            "\n"
            "Section \"Screen\"\n"
            "	Identifier \"Screen0\"\n"
            "	Device     \"Card0\"\n"
            "   SubSection \"Display\"\n"
            "       Depth     24"
            "   EndSubSection\n"
            "EndSection");

        config.WriteConfig();
    }
};

TEST_F(Xephyr24bppTest, CrashOn24bppHost)
{
    XORG_TESTCASE("Start Xephyr on a 24bpp host.\n"
                  "Verify Xephyr doesn't crash\n"
                  "https://bugzilla.redhat.com/show_bug.cgi?id=518960");

    ::Display *dpy = Display();
    int depth = DefaultDepth(dpy, 0);
    ASSERT_EQ(depth, 24);


    Process xephyr;
    setenv("DISPLAY", server.GetDisplayString().c_str(), 1);
    xephyr.Start("Xephyr", ":134", NULL);
    ASSERT_GT(xephyr.Pid(), 0);
    ASSERT_EQ(xephyr.GetState(), xorg::testing::Process::RUNNING);

    ::Display *ephdpy = NULL;

    int i = 0;
    while (i++ < 10 && !ephdpy) {
        ephdpy = XOpenDisplay(":134");
        if (!ephdpy)
            usleep(10000);
    }
    ASSERT_EQ(xephyr.GetState(), xorg::testing::Process::RUNNING);
    ASSERT_TRUE(ephdpy);

    xephyr.Terminate(1000);
}


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
