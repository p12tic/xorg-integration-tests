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
