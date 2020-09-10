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
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "xit-event.h"
#include "barriers-common.h"
#include "helpers.h"

#if HAVE_FIXES5

class BarrierSimpleTest : public BarrierTest {};

TEST_F(BarrierSimpleTest, CreateAndDestroyBarrier)
{
    XORG_TESTCASE("Create a valid pointer barrier.\n"
                  "Ensure PointerBarrier XID is valid.\n"
                  "Delete pointer barrier\n"
                  "Ensure no error is generated\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;

    XSync(dpy, False);

    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40,
                                         BarrierPositiveX, 0, NULL);
    ASSERT_NE((PointerBarrier)None, barrier) << "Failed to create barrier.";

    SetErrorTrap(dpy);
    XFixesDestroyPointerBarrier(dpy, barrier);
    ASSERT_NO_ERROR(ReleaseErrorTrap(dpy));
}

TEST_F(BarrierSimpleTest, DestroyInvalidBarrier)
{
    XORG_TESTCASE("Delete invalid pointer barrier\n"
                  "Ensure error is generated\n");

    SetErrorTrap(Display());
    XFixesDestroyPointerBarrier(Display(), -1);
    const XErrorEvent *error = ReleaseErrorTrap(Display());
    ASSERT_ERROR(error, xfixes_error_base + BadBarrier);
}

#if HAVE_XI23
TEST_F(BarrierSimpleTest, MultipleClientSecurity)
{
    XORG_TESTCASE("Ensure that two clients can't delete"
                  " each other's barriers.\n");

    ::Display *dpy1 = XOpenDisplay(server.GetDisplayString().c_str());
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    Window root = DefaultRootWindow(dpy1);

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy1, root, 20, 20, 20, 40, 0, 0, NULL);
    XSync(dpy1, False);

    SetErrorTrap(dpy2);
    XFixesDestroyPointerBarrier(dpy2, barrier);
    const XErrorEvent *error = ReleaseErrorTrap(dpy2);
    ASSERT_ERROR(error, BadAccess);

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_BarrierHit);
    XISelectEvents(dpy1, root, &mask, 1);
    XISelectEvents(dpy2, root, &mask, 1);
    delete[] mask.mask;

    XIWarpPointer(dpy1, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XSync(dpy1, False);
    XSync(dpy2, False);

    Dev(0).PlayRelMotion(-40, 0);

    XITEvent<XIBarrierEvent> event(dpy1, GenericEvent, xi2_opcode, XI_BarrierHit);
    ASSERT_EQ(XPending(dpy2), 0);

    SetErrorTrap(dpy2);
    XIBarrierReleasePointer(dpy2, VIRTUAL_CORE_POINTER_ID, event.ev->barrier, 1);
    error = ReleaseErrorTrap(dpy2);
    ASSERT_ERROR(error, BadAccess);
}
#endif

TEST_F(BarrierSimpleTest, PixmapsNotAllowed)
{
    XORG_TESTCASE("Pixmaps are not allowed as drawable.\n"
                  "Ensure error is generated\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    Pixmap p = XCreatePixmap(dpy, root, 10, 10, DefaultDepth(dpy, DefaultScreen(dpy)));
    XSync(dpy, False);

    SetErrorTrap(Display());
    XFixesCreatePointerBarrier(dpy, p, 20, 20, 20, 40, BarrierPositiveX, 0, NULL);
    const XErrorEvent *error = ReleaseErrorTrap(Display());
    ASSERT_ERROR(error, BadWindow);
    ASSERT_EQ(error->resourceid, p);
}

TEST_F(BarrierSimpleTest, InvalidDeviceCausesBadDevice)
{
    XORG_TESTCASE("Ensure that passing a garbage device ID\n"
                  "to XFixesCreatePointerBarrier returns with\n"
                  "a BadDevice error\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    int garbage_id = 0x00FACADE;
    const XErrorEvent *error;

    SetErrorTrap(dpy);
    XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, 1, &garbage_id);
    error = ReleaseErrorTrap(dpy);
    ASSERT_ERROR(error, xi_error_base + XI_BadDevice);
}

#define VALID_DIRECTIONS                                        \
    ::testing::Values(0L,                                       \
                      BarrierPositiveX,                         \
                      BarrierPositiveY,                         \
                      BarrierNegativeX,                         \
                      BarrierNegativeY,                         \
                      BarrierPositiveX | BarrierNegativeX,      \
                      BarrierPositiveY | BarrierNegativeX)

#define INVALID_DIRECTIONS                                              \
    ::testing::Values(BarrierPositiveX |                    BarrierPositiveY, \
                      BarrierNegativeX |                    BarrierNegativeY, \
                      BarrierPositiveX |                                       BarrierNegativeY, \
                      BarrierNegativeX | BarrierPositiveY,              \
                      BarrierPositiveX | BarrierNegativeX | BarrierPositiveY, \
                      BarrierPositiveX | BarrierNegativeX |                    BarrierNegativeY, \
                      BarrierPositiveX |                    BarrierPositiveY | BarrierNegativeY, \
                      BarrierNegativeX | BarrierPositiveY | BarrierNegativeY, \
                      BarrierPositiveX | BarrierNegativeX | BarrierPositiveY | BarrierNegativeY)


class BarrierZeroLength : public BarrierTest,
                          public ::testing::WithParamInterface<long int> {};
TEST_P(BarrierZeroLength, InvalidZeroLengthBarrier)
{
    XORG_TESTCASE("Create a pointer barrier with zero length.\n"
                  "Ensure server returns BadValue.");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    int directions = GetParam();

    XSync(dpy, False);

    SetErrorTrap(dpy);
    XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 20, directions, 0, NULL);
    const XErrorEvent *error = ReleaseErrorTrap(dpy);
    ASSERT_ERROR(error, BadValue);
}

INSTANTIATE_TEST_CASE_P(, BarrierZeroLength, VALID_DIRECTIONS);

class BarrierNonZeroArea : public BarrierTest,
                           public ::testing::WithParamInterface<long int> {};
TEST_P(BarrierNonZeroArea, InvalidNonZeroAreaBarrier)
{
    XORG_TESTCASE("Create pointer barrier with non-zero area.\n"
                  "Ensure server returns BadValue\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    int directions = GetParam();

    XSync(dpy, False);

    SetErrorTrap(dpy);
    XFixesCreatePointerBarrier(dpy, root, 20, 20, 40, 40, directions, 0, NULL);
    const XErrorEvent *error = ReleaseErrorTrap(dpy);
    ASSERT_ERROR(error, BadValue);
}
INSTANTIATE_TEST_CASE_P(, BarrierNonZeroArea, VALID_DIRECTIONS);

static void
BarrierPosForDirections(int directions, int *x1, int *y1, int *x2, int *y2)
{
    *x1 = 20;
    *y1 = 20;

    if (directions & BarrierPositiveY) {
        *x2 = 40;
        *y2 = 20;
    } else {
        *x2 = 20;
        *y2 = 40;
    }
}

class BarrierConflictingDirections : public BarrierTest,
                                     public ::testing::WithParamInterface<long int> {};
TEST_P(BarrierConflictingDirections, InvalidConflictingDirectionsBarrier)
{
    XORG_TESTCASE("Create a barrier with conflicting directions, such\n"
                  "as (PositiveX | NegativeY), and ensure that these\n"
                  "cases are handled properly.");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);

    int directions = GetParam();
    int x1, y1, x2, y2;
    BarrierPosForDirections(directions, &x1, &y1, &x2, &y2);

    XSync(dpy, False);

    SetErrorTrap(dpy);
    XFixesCreatePointerBarrier(dpy, root, x1, y1, x2, y2, directions, 0, NULL);
    const XErrorEvent *error = ReleaseErrorTrap(dpy);

    /* Nonsensical directions are ignored -- they don't
     * raise a BadValue. Unfortunately, there's no way
     * to query an existing pointer barrier, so we can't
     * actually check that the masked directions are proper.
     *
     * Just ensure we have no error.
     */

    ASSERT_NO_ERROR(error);
}
INSTANTIATE_TEST_CASE_P(, BarrierConflictingDirections, INVALID_DIRECTIONS);

#endif /* HAVE_FIXES5 */
