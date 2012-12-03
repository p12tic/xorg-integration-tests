#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/XInput2.h>

#include "barriers-common.h"
#include "xit-event.h"
#include "helpers.h"

using namespace xorg::testing::evemu;

class BarrierNotify : public BarrierTest {};

#define ASSERT_PTR_POS(x, y)                            \
    QueryPointerPosition(dpy, &root_x, &root_y);        \
    ASSERT_EQ(x, root_x);                               \
    ASSERT_EQ(y, root_y);

TEST_F(BarrierNotify, ReceivesNotifyEvents)
{
    XORG_TESTCASE("Ensure that we receive BarrierNotify events\n"
                  "when barriers are hit.\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);

    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, 0, NULL);
    XSync(dpy, False);

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask = reinterpret_cast<unsigned char*>(calloc(mask.mask_len, 1));
    XISetMask(mask.mask, XI_BarrierHit);
    XISelectEvents(dpy, root, &mask, 1);
    free(mask.mask);
    XSync(dpy, False);

    dev->PlayOne(EV_REL, REL_X, -40, True);

    /* Ensure we have a BarrierHit on our hands. */
    XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
    ASSERT_EQ(barrier, event.ev->barrier);
    ASSERT_EQ(20, event.ev->root_x);
    ASSERT_EQ(-40, event.ev->dx);

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_F(BarrierNotify, CorrectEventIDs)
{
    XORG_TESTCASE("Ensure that barrier event IDs have correct and "
                  "sequential values, and that multiple chained hits "
                  "have the same event ID\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);

    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, 0, NULL);
    XSync(dpy, False);

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask = reinterpret_cast<unsigned char*>(calloc(mask.mask_len, 1));
    XISetMask(mask.mask, XI_BarrierHit);
    XISetMask(mask.mask, XI_BarrierLeave);
    XISelectEvents(dpy, root, &mask, 1);
    free(mask.mask);
    XSync(dpy, False);

    /* Ensure we have a bunch of BarrierHits on our hands. */
    for (int i = 0; i < 10; i++) {
        dev->PlayOne(EV_REL, REL_X, -40, True);

        /* Ensure we have a BarrierHit on our hands. */
        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_EQ(barrier, event.ev->barrier);
        ASSERT_EQ(20, event.ev->root_x);
        ASSERT_EQ(30, event.ev->root_y);
        ASSERT_EQ(-40, event.ev->dx);
        ASSERT_EQ(0, event.ev->dy);
        ASSERT_EQ(1, event.ev->event_id);
    }

    /* Move outside the hitbox, and ensure that we
     * get a BarrierNewEvent */
    dev->PlayOne(EV_REL, REL_X, 20, True);
    XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierLeave);
    ASSERT_EQ(barrier, event.ev->barrier);
    ASSERT_EQ(2, event.ev->event_id);

    for (int i = 0; i < 10; i++) {
        dev->PlayOne(EV_REL, REL_X, -40, True);

        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_EQ(barrier, event.ev->barrier);
        ASSERT_EQ(2, event.ev->event_id);
    }

    /* Ensure that we're still inside the hit box. Event ID
     * should stay the same on such a minor change. */
    dev->PlayOne(EV_REL, REL_X, 1, True);

    for (int i = 0; i < 10; i++) {
        dev->PlayOne(EV_REL, REL_X, -40, True);

        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_EQ(barrier, event.ev->barrier);
        ASSERT_EQ(2, event.ev->event_id);
    }

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_F(BarrierNotify, BarrierReleases)
{
    XORG_TESTCASE("Ensure that releasing barriers works without "
                  "erroring out and allows pointer movement over "
                  "the barrier, and that we properly get a "
                  "XI_BarrierPointerReleased.\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask = reinterpret_cast<unsigned char*>(calloc(mask.mask_len, 1));
    XISetMask(mask.mask, XI_BarrierHit);
    XISetMask(mask.mask, XI_BarrierPointerReleased);
    XISetMask(mask.mask, XI_BarrierLeave);
    XISelectEvents(dpy, root, &mask, 1);
    free(mask.mask);
    XSync(dpy, False);

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);

    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, 0, NULL);
    XSync(dpy, False);

    dev->PlayOne(EV_REL, REL_X, -40, True);
    {
        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_EQ(barrier, event.ev->barrier);
        ASSERT_EQ(1, event.ev->event_id);
    }

    XIBarrierReleasePointer(dpy, barrier, 1, VIRTUAL_CORE_POINTER_ID);
    XSync(dpy, False);

    dev->PlayOne(EV_REL, REL_X, -40, True);
    {
        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierPointerReleased);
        ASSERT_EQ(barrier, event.ev->barrier);
        ASSERT_EQ(1, event.ev->event_id);
    }

    /* Immediately afterwards, we should have a new event
     * because we exited the hit box */
    {
        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierLeave);
        ASSERT_EQ(barrier, event.ev->barrier);
        ASSERT_EQ(2, event.ev->event_id);
    }

    XFixesDestroyPointerBarrier(dpy, barrier);
}
