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

#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/XInput2.h>

#include "barriers-common.h"
#include "helpers.h"

#if HAVE_FIXES5

using namespace xorg::testing::evemu;

static Bool
QueryPointerPosition(Display *dpy, int device_id,
                     double *root_x, double *root_y)
{
    Window root, child;
    double win_x, win_y;
    XIButtonState buttons;
    XIModifierState mods;
    XIGroupState group;

    return XIQueryPointer (dpy, device_id,
                           DefaultRootWindow (dpy),
                           &root, &child,
                           root_x, root_y,
                           &win_x, &win_y,
                           &buttons, &mods, &group);
 }

#define ASSERT_PTR_POS(dev, x, y)                       \
    QueryPointerPosition(dpy, dev, &root_x, &root_y);   \
    ASSERT_EQ(x, root_x);                               \
    ASSERT_EQ(y, root_y);

TEST_F(BarrierDevices, BarrierBlocksCorrectDevices)
{
    XORG_TESTCASE("Set up a vertical pointer barrier that\n"
                  "should block motion only on the VCP device.\n"
                  "Ensure that it blocks motion on the VCP only.\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;
    double root_x, root_y;

    /* Do some basic warps. */
    XIWarpPointer(dpy, master_id_1, None, root, 0, 0, 0, 0, 30, 30);
    ASSERT_PTR_POS(master_id_1, 30, 30);

    XIWarpPointer(dpy, master_id_2, None, root, 0, 0, 0, 0, 30, 30);
    ASSERT_PTR_POS(master_id_2, 30, 30);

    barrier = XFixesCreatePointerBarrier(dpy, root, 50, 0, 50, 50, 0, 1, &master_id_1);
    /* Ensure the barrier is created before we play events. */
    XSync(dpy, False);

    /* Make sure that the VCP is blocked when going across
     * the barrier, but the other device is not. */
    dev1->PlayOne(EV_REL, REL_X, 100, True);
    ASSERT_PTR_POS(master_id_1, 49, 30);

    dev2->PlayOne(EV_REL, REL_X, 100, True);
    ASSERT_PTR_POS(master_id_2, 130, 30);

    /* Assume that if all the other warp and directions tests
     * passed and were all correct than this holds true for the
     * multi-device case as well. If this is not the case, we can
     * always add more fanciness later. */

    XFixesDestroyPointerBarrier (dpy, barrier);
}
#endif /* HAVE_FIXES5 */
