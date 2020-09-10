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

class BarrierConstrained : public BarrierTest {};

#define ASSERT_PTR_POS(x, y)                            \
    QueryPointerPosition(dpy, &root_x, &root_y);        \
    ASSERT_EQ(x, root_x);                               \
    ASSERT_EQ(y, root_y);

/* Doing this parameterized would require a lot of fancy logic
 * that would make it super fancy. For now, just do it manually.
 */

TEST_F(BarrierConstrained, VerticalBarrierNoDirectionBlocksMotion)
{
    XORG_TESTCASE("Set up a vertical pointer barrier.\n"
                  "Ensure that mouse movement is blocked in both directions,\n"
                  " horizontally, when crossing the barrer and that warping\n"
                  " across the barrier is not blocked.");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;
    double root_x, root_y;

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 30);

    ASSERT_PTR_POS(30, 30);

    /* Barrier allows motion in neither direction */
    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, 0, NULL);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 10, 30);
    ASSERT_PTR_POS(10, 30);

    /* Ensure that absolute warping works fine. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 50, 30);
    ASSERT_PTR_POS(50, 30);

    /* This should be blocked. */
    Dev(0).PlayRelMotion(-100, 0);
    ASSERT_PTR_POS(20, 30);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 10, 30);
    ASSERT_PTR_POS(10, 30);

    /* This should be blocked. */
    Dev(0).PlayRelMotion(100, 0);
    ASSERT_PTR_POS(19, 30);

    XFixesDestroyPointerBarrier (dpy, barrier);
}

TEST_F(BarrierConstrained, VerticalBarrierPositiveXBlocksMotion)
{
    XORG_TESTCASE("Set up a vertical pointer barrier. Ensure that mouse"
                  " movement is blocked in the positive X direction when"
                  " crossing the barrer and that warping across the barrier is"
                  " not blocked.");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;
    double root_x, root_y;

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 30);

    ASSERT_PTR_POS(30, 30);

    /* Barrier allows motion in positive X */
    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40,
                                         BarrierPositiveX, 0, NULL);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 10, 30);
    ASSERT_PTR_POS(10, 30);

    /* Ensure that absolute warping works fine. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 50, 30);
    ASSERT_PTR_POS(50, 30);

    /* This should be blocked. */
    Dev(0).PlayRelMotion(-100, 0);
    ASSERT_PTR_POS(20, 30);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 10, 30);
    ASSERT_PTR_POS(10, 30);

    /* This shouldn't be blocked. */
    Dev(0).PlayRelMotion(100, 0);
    ASSERT_PTR_POS(110, 30);

    XFixesDestroyPointerBarrier (dpy, barrier);
}

TEST_F(BarrierConstrained, VerticalBarrierNegativeXBlocksMotion)
{
    XORG_TESTCASE("Set up a vertical pointer barrier. Ensure that mouse"
                  " movement is blocked in the negative X direction when"
                  " crossing the barrer and that warping across the barrier is"
                  " not blocked.");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;
    double root_x, root_y;

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 30);

    ASSERT_PTR_POS(30, 30);

    /* Barrier allows motion in negative X */
    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40,
                                         BarrierNegativeX, 0, NULL);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 10, 30);
    ASSERT_PTR_POS(10, 30);

    /* Ensure that absolute warping works fine. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 50, 30);
    ASSERT_PTR_POS(50, 30);

    /* This shouldn't be blocked. */
    Dev(0).PlayRelMotion(-100, 0);
    ASSERT_PTR_POS(0, 30);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 10, 30);
    ASSERT_PTR_POS(10, 30);

    /* This should be blocked. */
    Dev(0).PlayRelMotion(100, 0);
    ASSERT_PTR_POS(19, 30);

    XFixesDestroyPointerBarrier (dpy, barrier);
}

TEST_F(BarrierConstrained, VerticalBarrierBothDirectionsXBlocksNoMotion)
{
    XORG_TESTCASE("Set up a vertical pointer barrier. Ensure that mouse"
                  " movement is not blocked in either direction, horizontally, when"
                  " crossing the barrer and that warping across the barrier is"
                  " not blocked.");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;
    double root_x, root_y;

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 30);

    ASSERT_PTR_POS(30, 30);

    /* Barrier allows motion in negative X */
    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40,
                                         BarrierPositiveX | BarrierNegativeX,
                                         0, NULL);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 10, 30);
    ASSERT_PTR_POS(10, 30);

    /* Ensure that absolute warping works fine. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 50, 30);
    ASSERT_PTR_POS(50, 30);

    /* This shouldn't be blocked. */
    Dev(0).PlayRelMotion(-100, 0);
    ASSERT_PTR_POS(0, 30);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 10, 30);
    ASSERT_PTR_POS(10, 30);

    /* This shouldn't be blocked. */
    Dev(0).PlayRelMotion(100, 0);
    ASSERT_PTR_POS(110, 30);

    XFixesDestroyPointerBarrier (dpy, barrier);
}



TEST_F(BarrierConstrained, HorizontalBarrierNoDirectionBlocksMotion)
{
    XORG_TESTCASE("Set up a horizontal pointer barrier. Ensure that mouse"
                  " movement is blocked in both directions, vertically, when"
                  " crossing the barrer and that warping across the barrier is"
                  " not blocked.");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;
    double root_x, root_y;

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 30);

    ASSERT_PTR_POS(30, 30);

    /* Barrier allows motion in neither direction */
    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 40, 20, 0, 0, NULL);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 10);
    ASSERT_PTR_POS(30, 10);

    /* Ensure that absolute warping works fine. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 50);
    ASSERT_PTR_POS(30, 50);

    /* This should be blocked. */
    Dev(0).PlayRelMotion(0, -100);
    ASSERT_PTR_POS(30, 20);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 10);
    ASSERT_PTR_POS(30, 10);

    /* This should be blocked. */
    Dev(0).PlayRelMotion(0, 100);
    ASSERT_PTR_POS(30, 19);

    XFixesDestroyPointerBarrier (dpy, barrier);
}

TEST_F(BarrierConstrained, HorizontalBarrierPositiveYBlocksMotion)
{
    XORG_TESTCASE("Set up a horizontal pointer barrier. Ensure that mouse"
                  " movement is blocked in the positive Y direction when"
                  " crossing the barrer and that warping across the barrier is"
                  " not blocked.");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;
    double root_x, root_y;

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 30);

    ASSERT_PTR_POS(30, 30);

    /* Barrier allows motion in positive Y */
    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 40, 20,
                                         BarrierPositiveY, 0, NULL);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 10);
    ASSERT_PTR_POS(30, 10);

    /* Ensure that absolute warping works fine. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 50);
    ASSERT_PTR_POS(30, 50);

    /* This should be blocked. */
    Dev(0).PlayRelMotion(0, -100);
    ASSERT_PTR_POS(30, 20);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 10);
    ASSERT_PTR_POS(30, 10);

    /* This shouldn't be blocked. */
    Dev(0).PlayRelMotion(0, 100);
    ASSERT_PTR_POS(30, 110);

    XFixesDestroyPointerBarrier (dpy, barrier);
}

TEST_F(BarrierConstrained, HoritzontalBarrierNegativeYBlocksMotion)
{
    XORG_TESTCASE("Set up a horizontal pointer barrier. Ensure that mouse"
                  " movement is blocked in the negative Y direction when"
                  " crossing the barrer and that warping across the barrier is"
                  " not blocked.");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;
    double root_x, root_y;

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 30);

    ASSERT_PTR_POS(30, 30);

    /* Barrier allows motion in negative Y */
    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 40, 20,
                                         BarrierNegativeY, 0, NULL);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 10);
    ASSERT_PTR_POS(30, 10);

    /* Ensure that absolute warping works fine. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 50);
    ASSERT_PTR_POS(30, 50);

    /* This shouldn't be blocked. */
    Dev(0).PlayRelMotion(0, -100);
    ASSERT_PTR_POS(30, 0);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 10);
    ASSERT_PTR_POS(30, 10);

    /* This should be blocked. */
    Dev(0).PlayRelMotion(0, 100);
    ASSERT_PTR_POS(30, 19);

    XFixesDestroyPointerBarrier (dpy, barrier);
}

TEST_F(BarrierConstrained, HorizontalBarrierBothDirectionsYBlocksNoMotion)
{
    XORG_TESTCASE("Set up a horizontal pointer barrier. Ensure that mouse"
                  " movement is not blocked in either direction, vertically, when"
                  " crossing the barrer and that warping across the barrier is"
                  " not blocked.");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;
    double root_x, root_y;

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 30);

    ASSERT_PTR_POS(30, 30);

    /* Barrier allows motion in negative X */
    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 40, 20,
                                         BarrierPositiveY | BarrierNegativeY,
                                         0, NULL);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 10);
    ASSERT_PTR_POS(30, 10);

    /* Ensure that absolute warping works fine. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 50);
    ASSERT_PTR_POS(30, 50);

    /* This shouldn't be blocked. */
    Dev(0).PlayRelMotion(0, -100);
    ASSERT_PTR_POS(30, 0);

    /* Warp the pointer to before the barrier. */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID,
                  None, root, 0, 0, 0, 0, 30, 10);
    ASSERT_PTR_POS(30, 10);

    /* This shouldn't be blocked. */
    Dev(0).PlayRelMotion(0, 100);
    ASSERT_PTR_POS(30, 110);

    XFixesDestroyPointerBarrier (dpy, barrier);
}

#endif /* HAVE_FIXES5 */
