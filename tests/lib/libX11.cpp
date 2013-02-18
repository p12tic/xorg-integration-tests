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

#include <linux/input.h>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/Xlibint.h>

#include <xit-server-input-test.h>
#include <device-interface.h>

#include "helpers.h"
#include "xit-event.h"

/**
 * Test for libX11-related bugs.
 */
class libX11Test : public XITServerTest {
public:
    /**
     * Initializes a standard mouse device with two wheels.
     */
    virtual void SetUp() {
        XITServerTest::SetUp();
    }

    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.WriteConfig();
    }
};

TEST_F(libX11Test, MakeBigReqBufferOverflow)
{
    XORG_TESTCASE("Fill dpybuf until there are exactly 12 bytes left.\n"
                  "Request a PolyFillArc BigRequest\n"
                  "Verify that the request does not run outside the buffer"
                  "https://bugzilla.redhat.com/show_bug.cgi?id=56508");

    ::Display *dpy = Display();
    XSynchronize(dpy, False);
    Window root = DefaultRootWindow(dpy);
    GC gc = XCreateGC(dpy, root, 0, NULL);
    XSync(dpy, True);

    /* PolyFillArc is a 12 byte request, so fill the buffer with 4-byte
       requests until we align nicely.
       Buffer alignment we want:

         |     | < last 12 bytes> | ← bufmax
         [ ... |    PolyFillArc   ]

       MakeBigReq splits PolyFillArc into PFA1 (4 bytes) and PFA2 (8 bytes),
       inserting a 4 byte length field after PFA1 (after req->length).
       The rest is pushed back through a memmove.
       Bug 56508 shows the memmove is 4 bytes too much, so we get this:

         |     |         < last 12 bytes>      | ← bufmax
         |     | 0 1 2 3 | 4 5 6 7 | 8 9 10 11 | 12 13 14 15 ...
         [ ... |   PFA1  | br len  |          PFA2          |
                                                 ^ invalid write
     */
    int buflen = dpy->bufmax - dpy->bufptr; /* available buffer */
    while (buflen > 12) {
        XSetCloseDownMode(dpy, DestroyAll);
        buflen = dpy->bufmax - dpy->bufptr;
    }

    int req_len = (65535 * 4); /* size required for bigreq */
    int narcs = req_len/12 + 1;
    XArc arcs[narcs];

    char before = *dpy->bufmax; /* valgrind: invalid read error size 1 */

    XFillArcs(dpy, root, gc, arcs, narcs);

    char after = *dpy->bufmax; /* valgrind: invalid read error size 1 */

    /* Really, this is just sanity testing, we can't actually guarantee this
       fails. This test should be run in valgrind to make sure, watch out
       for:

       ==4145== Invalid write of size 2
       ==4145==    at 0x4A0A11E: memcpy@GLIBC_2.2.5 (mc_replace_strmem.c:880)
       ==4145==    by 0x54A1ADE: XFillArcs (FillArcs.c:58)
       ==4145==    by 0x40625B: libX11Test_MakeBigReqBufferOverflow_Test::TestBody() (libX11.cpp:87)
    */
    ASSERT_EQ(before, after);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

