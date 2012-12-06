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
                  "XI_BarrierHit with the correct flags.\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask = reinterpret_cast<unsigned char*>(calloc(mask.mask_len, 1));
    XISetMask(mask.mask, XI_BarrierHit);
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
        ASSERT_FALSE((event.ev->flags & XIBarrierPointerReleased));
    }

    XIBarrierReleasePointer(dpy, VIRTUAL_CORE_POINTER_ID, barrier, 1);
    XSync(dpy, False);

    dev->PlayOne(EV_REL, REL_X, -40, True);

    /* We should have a new event because we exited the hit box */
    {
        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierLeave);
        ASSERT_EQ(barrier, event.ev->barrier);
        ASSERT_EQ(2, event.ev->event_id);
        ASSERT_TRUE((event.ev->flags & XIBarrierPointerReleased));
    }

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_F(BarrierNotify, DestroyWindow)
{
    XORG_TESTCASE("Create a window.\n"
                  "Set up a barrier using this window as drawable.\n"
                  "Select for barrier events.\n"
                  "Ensure events are received\n"
                  "Destroy the window.\n"
                  "Ensure events are not received anymore.\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    Window win = CreateWindow(dpy, root);

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, win, 20, 20, 20, 40, 0, 0, NULL);
    XSync(dpy, False);

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask = reinterpret_cast<unsigned char*>(calloc(mask.mask_len, 1));
    XISetMask(mask.mask, XI_BarrierHit);
    XISelectEvents(dpy, win, &mask, 1);
    free(mask.mask);
    XSync(dpy, False);

    dev->PlayOne(EV_REL, REL_X, -40, True);

    /* Ensure we have a BarrierHit on our hands. */
    XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
    ASSERT_EQ(barrier, event.ev->barrier);
    ASSERT_EQ(20, event.ev->root_x);
    ASSERT_EQ(-40, event.ev->dx);

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XDestroyWindow(dpy, win);
    XSync(dpy, True);

    dev->PlayOne(EV_REL, REL_X, -40, True);

    double x, y;
    QueryPointerPosition(dpy, &x, &y);
    EXPECT_EQ(x, 20);
    EXPECT_EQ(y, 30);

    ASSERT_FALSE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                            GenericEvent,
                                                            xi2_opcode,
                                                            XI_BarrierHit,
                                                            500));

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_F(BarrierNotify, UnmapWindow)
{
    XORG_TESTCASE("Create a window.\n"
                  "Set up a barrier using this window as drawable.\n"
                  "Select for barrier events.\n"
                  "Ensure events are received\n"
                  "Unmap the window.\n"
                  "Ensure events are still received.\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    Window win = CreateWindow(dpy, root);

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, win, 20, 20, 20, 40, 0, 0, NULL);
    XSync(dpy, False);

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask = reinterpret_cast<unsigned char*>(calloc(mask.mask_len, 1));
    XISetMask(mask.mask, XI_BarrierHit);
    XISelectEvents(dpy, win, &mask, 1);
    free(mask.mask);
    XSync(dpy, False);

    dev->PlayOne(EV_REL, REL_X, -40, True);

    /* Ensure we have a BarrierHit on our hands. */
    XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
    ASSERT_EQ(barrier, event.ev->barrier);
    ASSERT_EQ(20, event.ev->root_x);
    ASSERT_EQ(-40, event.ev->dx);

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XUnmapWindow(dpy, win);
    XSync(dpy, True);

    dev->PlayOne(EV_REL, REL_X, -40, True);

    double x, y;
    QueryPointerPosition(dpy, &x, &y);
    EXPECT_EQ(x, 20);
    EXPECT_EQ(y, 30);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_BarrierHit,
                                                           500));
    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_F(BarrierNotify, EventsDuringActiveGrab)
{
    XORG_TESTCASE("Set up a pointer barrier.\n"
                  "Actively grab the pointer.\n"
                  "Move pointer against barrier.\n"
                  "Expect events\n");
    /* variations
       TODO: - core, xi2  (xi1 not needed)
       - barrier event masks set in grab mask
       - owner_events true/false
       TODO: - grab window == barrier window or other window

       if OE is true and mask is set → event
       if OE is false and mask is set → event
       if OE is true and mask is not set, but set on window → event
       if OE is false and mask is not set, but set on window → no event
     */

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);

    unsigned char event_mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask event_mask = { XIAllMasterDevices, sizeof (event_mask_bits), event_mask_bits };
    XISetMask(event_mask_bits, XI_BarrierHit);

    unsigned char no_mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask no_mask = { XIAllMasterDevices, sizeof (no_mask_bits), no_mask_bits };

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, 0, NULL);
    XSync(dpy, False);

    /* if OE is true and mask is not set, but set on window → event */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &event_mask, 1);
    XIGrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, root, CurrentTime, None, GrabModeAsync, GrabModeAsync, True, &no_mask);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->window, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
    }

    /* if OE is false and mask is not set, but set on window → no event */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &event_mask, 1);
    XIGrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, root, CurrentTime, None, GrabModeAsync, GrabModeAsync, False, &no_mask);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_FALSE(xorg::testing::XServer::WaitForEvent(dpy, 500));

    /* if OE is true and mask is set → event */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &no_mask, 1);
    XIGrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, root, CurrentTime, None, GrabModeAsync, GrabModeAsync, True, &event_mask);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->window, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
    }

    /* if OE is false and mask is set → event */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &no_mask, 1);
    XIGrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, root, CurrentTime, None, GrabModeAsync, GrabModeAsync, False, &event_mask);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->window, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
    }

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_F(BarrierNotify, EventsDuringActiveGrabNonGrabWindow)
{
    XORG_TESTCASE("Set up a pointer barrier on the root window.\n"
                  "Create a window\n"
                  "Actively grab the pointer on the window.\n"
                  "Move pointer against barrier.\n"
                  "Expect events\n");

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    Window win = CreateWindow(dpy, root);

    unsigned char event_mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask event_mask = { XIAllMasterDevices, sizeof (event_mask_bits), event_mask_bits };
    XISetMask(event_mask_bits, XI_BarrierHit);

    unsigned char no_mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask no_mask = { XIAllMasterDevices, sizeof (no_mask_bits), no_mask_bits };

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, 0, NULL);
    XSync(dpy, False);

    /* expect event regardless on owner_events setting */
    /* if OE is true and mask is not set, but set on window → event */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &event_mask, 1);
    XIGrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, win, CurrentTime, None, GrabModeAsync, GrabModeAsync, True, &no_mask);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->window, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
    }

    /* if OE is false and mask is not set, but set on window → event */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XIGrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, win, CurrentTime, None, GrabModeAsync, GrabModeAsync, False, &no_mask);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->window, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
    }

    /* if OE is true and mask is set → event */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XIGrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, win, CurrentTime, None, GrabModeAsync, GrabModeAsync, True, &event_mask);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->window, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
    }

    /* if OE is false and mask is set → event */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XIGrabDevice(dpy, VIRTUAL_CORE_POINTER_ID, win, CurrentTime, None, GrabModeAsync, GrabModeAsync, False, &event_mask);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->window, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
    }

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_F(BarrierNotify, EventsDuringActiveGrabOtherClient)
{
    XORG_TESTCASE("Set up a pointer barrier.\n"
                  "Have another client actively grab the pointer.\n"
                  "Move pointer against barrier.\n"
                  "Expect events\n");

    ::Display *dpy = Display();
    ::Display *dpy2 = XOpenDisplay(server.GetDisplayString().c_str());
    XSynchronize(dpy, True);
    XSynchronize(dpy2, True);
    Window root = DefaultRootWindow(dpy);

    unsigned char event_mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask event_mask = { XIAllMasterDevices, sizeof (event_mask_bits), event_mask_bits };
    XISetMask(event_mask_bits, XI_BarrierHit);

    unsigned char no_mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask no_mask = { XIAllMasterDevices, sizeof (no_mask_bits), no_mask_bits };

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, 0, NULL);
    XSync(dpy, False);

    /* second client grabs device */
    XIGrabDevice(dpy2, VIRTUAL_CORE_POINTER_ID, root, CurrentTime, None, GrabModeAsync, GrabModeAsync, True, &no_mask);

    /* We still expect events */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &event_mask, 1);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->window, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
    }

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_F(BarrierNotify, EventsDuringPassiveGrab)
{
    XORG_TESTCASE("Set up a pointer barrier.\n"
                  "Trigger a passive pointer grab\n"
                  "Move pointer against barrier.\n"
                  "Expect events\n");
    /* variations
       TODO: - core, xi2  (xi1 not needed) 
       - barrier event masks set in grab mask
       - owner_events true/false
       TODO: - grab window == barrier window or other window

       if OE is true and mask is set → event
       if OE is false and mask is set → event
       if OE is true and mask is not set, but set on window → event
       if OE is false and mask is not set, but set on window → no event
     */

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);

    XIGrabModifiers mods = { (int)XIAnyModifier, 0 };

    unsigned char event_mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask event_mask = { XIAllMasterDevices, sizeof (event_mask_bits), event_mask_bits };
    XISetMask(event_mask_bits, XI_BarrierHit);

    unsigned char no_mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask no_mask = { XIAllMasterDevices, sizeof (no_mask_bits), no_mask_bits };

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, 0, NULL);
    XSync(dpy, False);

    /* if OE is true and mask is not set, but set on window → event */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &event_mask, 1);
    XIGrabButton(dpy, VIRTUAL_CORE_POINTER_ID, 1, root, None, GrabModeAsync, GrabModeAsync, True, &no_mask, 1, &mods);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, True);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->window, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
    }
    XIUngrabButton(dpy, VIRTUAL_CORE_POINTER_ID, 1, root, 1, &mods);

    /* if OE is false and mask is not set, but set on window → no event */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &event_mask, 1);
    XIGrabButton(dpy, VIRTUAL_CORE_POINTER_ID, 1, root, None, GrabModeAsync, GrabModeAsync, False, &no_mask, 1, &mods);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, True);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, True);
    ASSERT_FALSE(xorg::testing::XServer::WaitForEvent(dpy, 500));
    XIUngrabButton(dpy, VIRTUAL_CORE_POINTER_ID, 1, root, 1, &mods);

    /* if OE is true and mask is set → event */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &no_mask, 1);
    XIGrabButton(dpy, VIRTUAL_CORE_POINTER_ID, 1, root, None, GrabModeAsync, GrabModeAsync, True, &event_mask, 1, &mods);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, True);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->window, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
    }
    XIUngrabButton(dpy, VIRTUAL_CORE_POINTER_ID, 1, root, 1, &mods);

    /* if OE is false and mask is set → event */
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &no_mask, 1);
    XIGrabButton(dpy, VIRTUAL_CORE_POINTER_ID, 1, root, None, GrabModeAsync, GrabModeAsync, False, &event_mask, 1, &mods);
    dev->PlayOne(EV_KEY, BTN_LEFT, 1, True);
    dev->PlayOne(EV_REL, REL_X, -40, True);
    dev->PlayOne(EV_KEY, BTN_LEFT, 0, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->window, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
    }
    XIUngrabButton(dpy, VIRTUAL_CORE_POINTER_ID, 1, root, 1, &mods);

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_F(BarrierNotify, BarrierRandREventsVertical)
{
    XORG_TESTCASE("Set up pointer barrier close to a screen.\n"
                  "Move the pointer against the barrier that the barrier blocks movement\n"
                  "Same movement must be restriced by the RandR dimensions"
                  "Ensure Barrier event root x/y are fully constrained");
#if 0
             +-------------+
             |             |
             |             |
             |             |
             |         a | |
             |          y| |
             +-----------+-+
                        x|
                         |b

     Move a→b will clamp the barrier at X, but we want Y (i.e. including
     RandR clamping)

#endif

    ::Display *dpy = Display();
    XSynchronize(dpy, True);
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;

    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LASTEVENT);
    mask.mask = new unsigned char[mask.mask_len]();
    XISetMask(mask.mask, XI_BarrierHit);
    XISetMask(mask.mask, XI_BarrierLeave);
    XISelectEvents(dpy, root, &mask, 1);
    delete[] mask.mask;

    int w = DisplayWidth(dpy, DefaultScreen(dpy));
    int h = DisplayHeight(dpy, DefaultScreen(dpy));

    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, root, 0, 0, 0, 0, w - 40 , h - 30);

    barrier = XFixesCreatePointerBarrier(dpy, root, w - 20, 0, w - 20, 4000, 0, 0, NULL);

    dev->PlayOne(EV_REL, REL_X, 40, false);
    dev->PlayOne(EV_REL, REL_Y, 100, true);

    XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
    ASSERT_EQ(barrier, event.ev->barrier);
    ASSERT_EQ(1, event.ev->event_id);
    ASSERT_EQ(event.ev->root_x, w - 20 - 1);
    ASSERT_LT(event.ev->root_y, h);
}
