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

#if HAVE_XI23

using namespace xorg::testing::evemu;

enum BarrierDeviceTestCombinations {
    NO_DEVICE_SPECIFICS, /**< test with no barrier device ID specified */
    VCP_ONLY,            /**< test for VCP, only one MD */
    TARGET_VCP,          /**< test for VCP with two MDs present */
    TARGET_POINTER2,     /**< test for second pointer with two MDs present */
    TARGET_VCP_AND_ALL,  /**< select for 2 MDs, but use VCP as device */
    TARGET_POINTER2_AND_ALL, /**< select for 2 MDs, but use second MD as device */
    LATE_SECOND_MD_VCP,  /**< create second MD after pointer barrier, but
                              use VCP as moving device */
    LATE_SECOND_MD_POINTER2,  /**< create second MD after pointer barrier, but
                                   use second MD as moving device */
};

static std::string enum_to_string(enum BarrierDeviceTestCombinations b) {
    switch(b) {
        case NO_DEVICE_SPECIFICS: return "NO_DEVICE_SPECIFICS";
        case VCP_ONLY:            return "VCP_ONLY";
        case TARGET_VCP:          return "TARGET_VCP";
        case TARGET_POINTER2:     return "TARGET_POINTER2";
        case TARGET_VCP_AND_ALL:  return "TARGET_VCP_AND_ALL";
        case TARGET_POINTER2_AND_ALL:     return "TARGET_POINTER2_AND_ALL";
        case LATE_SECOND_MD_VCP:  return "LATE_SECOND_MD_VCP";
        case LATE_SECOND_MD_POINTER2:     return "LATE_SECOND_MD_POINTER2";
    }

    ADD_FAILURE() << "Bug. we shouldn't get here.";
    return "";
}

class BarrierNotify : public BarrierDevices,
                      public ::testing::WithParamInterface<enum BarrierDeviceTestCombinations> {
public:
    std::auto_ptr<Device>* target_dev;
    int ndevices;
    int deviceid;
    int sourceid;
    int all_deviceids[2];

    virtual void SetUp(){
        SetUpDevices();
        XITServerInputTest::SetUp();

        switch(GetParam()) {
            case NO_DEVICE_SPECIFICS:
            case VCP_ONLY:
            case LATE_SECOND_MD_VCP:
            case LATE_SECOND_MD_POINTER2:
                break;

            /* Set up 2 MDs */
            case TARGET_VCP:
            case TARGET_POINTER2:
            case TARGET_VCP_AND_ALL:
            case TARGET_POINTER2_AND_ALL:
                ConfigureDevices();
                break;
            default:
                FAIL();
                break;
        }

        SetDeviceValues(GetParam());
    }

    virtual void SelectBarrierEvents(::Display *dpy, Window win) {
        XIEventMask mask;
        mask.deviceid = XIAllMasterDevices;
        mask.mask_len = XIMaskLen(XI_LASTEVENT);
        mask.mask = new unsigned char[mask.mask_len]();
        XISetMask(mask.mask, XI_BarrierHit);
        XISetMask(mask.mask, XI_BarrierLeave);
        XISelectEvents(dpy, win, &mask, 1);
        delete[] mask.mask;
        XSync(dpy, False);
    }

    virtual void SetDeviceValues(enum BarrierDeviceTestCombinations combination)
    {
        const char *source_dev;

        switch(combination) {
            case NO_DEVICE_SPECIFICS:
                target_dev = &dev1;
                deviceid = VIRTUAL_CORE_POINTER_ID;
                ndevices = 0;
                source_dev = "--device1--";
                break;
            case TARGET_VCP:
            case VCP_ONLY:
                ndevices = 1;
                deviceid = VIRTUAL_CORE_POINTER_ID;
                target_dev = &dev1;
                source_dev = "--device1--";
                all_deviceids[0] = VIRTUAL_CORE_POINTER_ID;
                break;
            case TARGET_POINTER2:
                ndevices = 1;
                deviceid = master_id_2;
                target_dev = &dev2;
                source_dev = "--device2--";
                all_deviceids[0] = master_id_2;
                break;
            case TARGET_VCP_AND_ALL:
                ndevices = 2;
                deviceid = VIRTUAL_CORE_POINTER_ID;
                target_dev = &dev1;
                source_dev = "--device1--";
                all_deviceids[0] = VIRTUAL_CORE_POINTER_ID;
                all_deviceids[1] = master_id_2;
                break;
            case TARGET_POINTER2_AND_ALL:
                ndevices = 2;
                deviceid = master_id_2;
                target_dev = &dev2;
                source_dev = "--device2--";
                all_deviceids[0] = VIRTUAL_CORE_POINTER_ID;
                all_deviceids[1] = master_id_2;
                break;
            case LATE_SECOND_MD_VCP:
                ndevices = 0;
                deviceid = -1;
                target_dev = &dev1;
                source_dev = "--device1--";
                break;
            case LATE_SECOND_MD_POINTER2:
                ndevices = 0;
                deviceid = -1;
                target_dev = &dev2;
                source_dev = "--device2--";
                break;
        }

        ASSERT_TRUE(FindInputDeviceByName(Display(), source_dev, &sourceid)) << "Failed to find device.";
    }

    virtual void ConfigureLateDevices(enum BarrierDeviceTestCombinations combination)
    {
        switch(combination) {
            case NO_DEVICE_SPECIFICS:
            case VCP_ONLY:
            case TARGET_VCP:
            case TARGET_POINTER2:
            case TARGET_VCP_AND_ALL:
            case TARGET_POINTER2_AND_ALL:
                break;
            case LATE_SECOND_MD_VCP:
                ConfigureDevices();
                deviceid = VIRTUAL_CORE_POINTER_ID;
                break;
            case LATE_SECOND_MD_POINTER2:
                ConfigureDevices();
                deviceid = master_id_2;
                break;
            default:
                FAIL();
                break;
        }
    }
};

#define ASSERT_PTR_POS(x, y)                            \
    QueryPointerPosition(dpy, &root_x, &root_y);        \
    ASSERT_EQ(x, root_x);                               \
    ASSERT_EQ(y, root_y);

TEST_P(BarrierNotify, ReceivesNotifyEvents)
{
    enum BarrierDeviceTestCombinations combination = GetParam();

    XORG_TESTCASE("Ensure that we receive BarrierNotify events\n"
                  "when barriers are hit.\n");
    SCOPED_TRACE(enum_to_string(combination));

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;

    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, ndevices, all_deviceids);

    SelectBarrierEvents(dpy, root);

    ConfigureLateDevices(combination);

    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XSync(dpy, False);

    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);

    /* Ensure we have a BarrierHit on our hands. */
    XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
    ASSERT_TRUE(event.ev);
    ASSERT_EQ(barrier, event.ev->barrier);
    ASSERT_EQ(20, event.ev->root_x);
    ASSERT_EQ(-40, event.ev->dx);
    ASSERT_EQ(event.ev->deviceid, deviceid);
    ASSERT_EQ(event.ev->sourceid, sourceid);

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_P(BarrierNotify, CorrectEventIDs)
{
    enum BarrierDeviceTestCombinations combination = GetParam();

    XORG_TESTCASE("Ensure that barrier event IDs have correct and "
                  "sequential values, and that multiple chained hits "
                  "have the same event ID\n");
    SCOPED_TRACE(enum_to_string(combination));

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;

    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, ndevices, all_deviceids);

    SelectBarrierEvents(dpy, root);

    ConfigureLateDevices(combination);

    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XSync(dpy, False);

    /* Ensure we have a bunch of BarrierHits on our hands. */
    for (int i = 0; i < 10; i++) {
        (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);

        /* Ensure we have a BarrierHit on our hands. */
        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(event.ev);
        ASSERT_EQ(barrier, event.ev->barrier);
        ASSERT_EQ(20, event.ev->root_x);
        ASSERT_EQ(30, event.ev->root_y);
        ASSERT_EQ(-40, event.ev->dx);
        ASSERT_EQ(0, event.ev->dy);
        ASSERT_EQ(1U, event.ev->eventid);
        ASSERT_EQ(event.ev->deviceid, deviceid);
        ASSERT_EQ(event.ev->sourceid, sourceid);
    }

    /* Move outside the hitbox, and ensure that we
     * get a BarrierLeave */
    (*target_dev)->PlayOne(EV_REL, REL_X, 20, True);
    XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierLeave);
    ASSERT_EQ(barrier, event.ev->barrier);
    ASSERT_EQ(1U, event.ev->eventid);

    for (int i = 0; i < 10; i++) {
        (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);

        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(event.ev);
        ASSERT_EQ(barrier, event.ev->barrier);
        ASSERT_EQ(2U, event.ev->eventid);
        ASSERT_EQ(event.ev->deviceid, deviceid);
        ASSERT_EQ(event.ev->sourceid, sourceid);
    }

    /* Ensure that we're still inside the hit box. Event ID
     * should stay the same on such a minor change. */
    (*target_dev)->PlayOne(EV_REL, REL_X, 1, True);

    for (int i = 0; i < 10; i++) {
        (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);

        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_EQ(barrier, event.ev->barrier);
        ASSERT_EQ(2U, event.ev->eventid);
        ASSERT_EQ(event.ev->deviceid, deviceid);
        ASSERT_EQ(event.ev->sourceid, sourceid);
    }

    XFixesDestroyPointerBarrier(dpy, barrier);
}


TEST_P(BarrierNotify, BarrierReleases)
{
    enum BarrierDeviceTestCombinations combination = GetParam();
    XORG_TESTCASE("Ensure that releasing barriers works without "
                  "erroring out and allows pointer movement over "
                  "the barrier, and that we properly get a "
                  "XI_BarrierHit with the correct flags.\n");
    SCOPED_TRACE(enum_to_string(combination));

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    PointerBarrier barrier;

    SelectBarrierEvents(dpy, root);

    barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, ndevices, all_deviceids);

    ConfigureLateDevices(combination);

    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XSync(dpy, False);

    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    {
        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(event.ev);
        ASSERT_EQ(barrier, event.ev->barrier);
        ASSERT_EQ(1U, event.ev->eventid);
        ASSERT_FALSE((event.ev->flags & XIBarrierPointerReleased));
        ASSERT_EQ(event.ev->deviceid, deviceid);
        ASSERT_EQ(event.ev->sourceid, sourceid);
    }

    XIBarrierReleasePointer(dpy, deviceid, barrier, 1);
    XSync(dpy, False);

    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);

    /* We should have a new event because we exited the hit box */
    {
        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierLeave);
        ASSERT_TRUE(event.ev);
        ASSERT_EQ(barrier, event.ev->barrier);
        ASSERT_EQ(1U, event.ev->eventid);
        ASSERT_TRUE((event.ev->flags & XIBarrierPointerReleased));
        ASSERT_EQ(event.ev->deviceid, deviceid);
        ASSERT_EQ(event.ev->sourceid, sourceid);
    }

    XFixesDestroyPointerBarrier(dpy, barrier);

    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_P(BarrierNotify, DestroyWindow)
{
    enum BarrierDeviceTestCombinations combination = GetParam();
    XORG_TESTCASE("Create a window.\n"
                  "Set up a barrier using this window as drawable.\n"
                  "Select for barrier events.\n"
                  "Ensure events are received\n"
                  "Destroy the window.\n"
                  "Ensure events are not received anymore.\n");
    SCOPED_TRACE(enum_to_string(combination));

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    Window win = CreateWindow(dpy, root);


    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, win, 20, 20, 20, 40, 0, ndevices, all_deviceids);

    SelectBarrierEvents(dpy, win);

    ConfigureLateDevices(combination);

    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XSync(dpy, False);

    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);

    /* Ensure we have a BarrierHit on our hands. */
    XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
    ASSERT_TRUE(event.ev);
    ASSERT_EQ(barrier, event.ev->barrier);
    ASSERT_EQ(20, event.ev->root_x);
    ASSERT_EQ(-40, event.ev->dx);
    ASSERT_EQ(event.ev->deviceid, deviceid);
    ASSERT_EQ(event.ev->sourceid, sourceid);

    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XDestroyWindow(dpy, win);
    XSync(dpy, True);

    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);

    double x, y;
    QueryPointerPosition(dpy, &x, &y, deviceid);
    EXPECT_EQ(x, 20);
    EXPECT_EQ(y, 30);

    ASSERT_FALSE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                            GenericEvent,
                                                            xi2_opcode,
                                                            XI_BarrierHit,
                                                            500));

    ASSERT_TRUE(NoEventPending(dpy));
    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_P(BarrierNotify, UnmapWindow)
{
    enum BarrierDeviceTestCombinations combination = GetParam();
    XORG_TESTCASE("Create a window.\n"
                  "Set up a barrier using this window as drawable.\n"
                  "Select for barrier events.\n"
                  "Ensure events are received\n"
                  "Unmap the window.\n"
                  "Ensure events are still received.\n");
    SCOPED_TRACE(enum_to_string(combination));

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    Window win = CreateWindow(dpy, root);

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, win, 20, 20, 20, 40, 0, ndevices, all_deviceids);

    SelectBarrierEvents(dpy, win);

    ConfigureLateDevices(combination);

    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XSync(dpy, False);

    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);

    /* Ensure we have a BarrierHit on our hands. */
    XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
    ASSERT_TRUE(event.ev);
    ASSERT_EQ(barrier, event.ev->barrier);
    ASSERT_EQ(20, event.ev->root_x);
    ASSERT_EQ(-40, event.ev->dx);
    ASSERT_EQ(event.ev->deviceid, deviceid);
    ASSERT_EQ(event.ev->sourceid, sourceid);

    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XUnmapWindow(dpy, win);
    XSync(dpy, True);

    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);

    double x, y;
    QueryPointerPosition(dpy, &x, &y, deviceid);
    EXPECT_EQ(x, 20);
    EXPECT_EQ(y, 30);

    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(Display(),
                                                           GenericEvent,
                                                           xi2_opcode,
                                                           XI_BarrierHit,
                                                           500));
    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_P(BarrierNotify, EventsDuringActiveGrab)
{
    enum BarrierDeviceTestCombinations combination = GetParam();
    XORG_TESTCASE("Set up a pointer barrier.\n"
                  "Actively grab the pointer.\n"
                  "Move pointer against barrier.\n"
                  "Expect events\n");
    SCOPED_TRACE(enum_to_string(combination));
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

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, ndevices, all_deviceids);
    XSync(dpy, False);

    ConfigureLateDevices(combination);

    /* if OE is true and mask is not set, but set on window → event */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &event_mask, 1);
    XIGrabDevice(dpy, deviceid, root, CurrentTime, None, GrabModeAsync, GrabModeAsync, True, &no_mask);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->event, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
        ASSERT_EQ(hit.ev->deviceid, deviceid);
        ASSERT_EQ(hit.ev->sourceid, sourceid);
        ASSERT_TRUE((hit.ev->flags & XIBarrierDeviceIsGrabbed));
    }

    /* if OE is false and mask is not set, but set on window → no event */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &event_mask, 1);
    XIGrabDevice(dpy, deviceid, root, CurrentTime, None, GrabModeAsync, GrabModeAsync, False, &no_mask);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_FALSE(xorg::testing::XServer::WaitForEvent(dpy, 500));

    /* if OE is true and mask is set → event */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &no_mask, 1);
    XIGrabDevice(dpy, deviceid, root, CurrentTime, None, GrabModeAsync, GrabModeAsync, True, &event_mask);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->event, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
        ASSERT_EQ(hit.ev->deviceid, deviceid);
        ASSERT_EQ(hit.ev->sourceid, sourceid);
        ASSERT_TRUE((hit.ev->flags & XIBarrierDeviceIsGrabbed));
    }

    /* if OE is false and mask is set → event */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &no_mask, 1);
    XIGrabDevice(dpy, deviceid, root, CurrentTime, None, GrabModeAsync, GrabModeAsync, False, &event_mask);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->event, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
        ASSERT_EQ(hit.ev->deviceid, deviceid);
        ASSERT_EQ(hit.ev->sourceid, sourceid);
        ASSERT_TRUE((hit.ev->flags & XIBarrierDeviceIsGrabbed));
    }

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_P(BarrierNotify, EventsDuringActiveGrabNonGrabWindow)
{
    enum BarrierDeviceTestCombinations combination = GetParam();
    XORG_TESTCASE("Set up a pointer barrier on the root window.\n"
                  "Create a window\n"
                  "Actively grab the pointer on the window.\n"
                  "Move pointer against barrier.\n"
                  "Expect events\n");
    SCOPED_TRACE(enum_to_string(combination));

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    Window win = CreateWindow(dpy, root);

    unsigned char event_mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask event_mask = { XIAllMasterDevices, sizeof (event_mask_bits), event_mask_bits };
    XISetMask(event_mask_bits, XI_BarrierHit);

    unsigned char no_mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask no_mask = { XIAllMasterDevices, sizeof (no_mask_bits), no_mask_bits };

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, ndevices, all_deviceids);
    XSync(dpy, False);

    ConfigureLateDevices(combination);

    /* expect event regardless on owner_events setting */
    /* if OE is true and mask is not set, but set on window → event */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &event_mask, 1);
    XIGrabDevice(dpy, deviceid, win, CurrentTime, None, GrabModeAsync, GrabModeAsync, True, &no_mask);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->event, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
        ASSERT_EQ(hit.ev->deviceid, deviceid);
        ASSERT_EQ(hit.ev->sourceid, sourceid);
        ASSERT_TRUE((hit.ev->flags & XIBarrierDeviceIsGrabbed));
    }

    /* if OE is false and mask is not set, but set on window → event */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XIGrabDevice(dpy, deviceid, win, CurrentTime, None, GrabModeAsync, GrabModeAsync, False, &no_mask);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->event, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
        ASSERT_EQ(hit.ev->deviceid, deviceid);
        ASSERT_EQ(hit.ev->sourceid, sourceid);
        ASSERT_TRUE((hit.ev->flags & XIBarrierDeviceIsGrabbed));
    }

    /* if OE is true and mask is set → event */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XIGrabDevice(dpy, deviceid, win, CurrentTime, None, GrabModeAsync, GrabModeAsync, True, &event_mask);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->event, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
        ASSERT_EQ(hit.ev->deviceid, deviceid);
        ASSERT_EQ(hit.ev->sourceid, sourceid);
        ASSERT_TRUE((hit.ev->flags & XIBarrierDeviceIsGrabbed));
    }

    /* if OE is false and mask is set → event */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XIGrabDevice(dpy, deviceid, win, CurrentTime, None, GrabModeAsync, GrabModeAsync, False, &event_mask);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->event, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
        ASSERT_EQ(hit.ev->deviceid, deviceid);
        ASSERT_EQ(hit.ev->sourceid, sourceid);
        ASSERT_TRUE((hit.ev->flags & XIBarrierDeviceIsGrabbed));
    }

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_P(BarrierNotify, EventsDuringActiveGrabOtherClient)
{
    enum BarrierDeviceTestCombinations combination = GetParam();
    XORG_TESTCASE("Set up a pointer barrier.\n"
                  "Have another client actively grab the pointer.\n"
                  "Move pointer against barrier.\n"
                  "Expect events\n");
    SCOPED_TRACE(enum_to_string(combination));

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

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, ndevices, all_deviceids);
    XSync(dpy, False);

    ConfigureLateDevices(combination);

    /* second client grabs device */
    XIGrabDevice(dpy2, deviceid, root, CurrentTime, None, GrabModeAsync, GrabModeAsync, True, &no_mask);

    /* We still expect events */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &event_mask, 1);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->event, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
        ASSERT_EQ(hit.ev->deviceid, deviceid);
        ASSERT_EQ(hit.ev->sourceid, sourceid);
        ASSERT_TRUE((hit.ev->flags & XIBarrierDeviceIsGrabbed));
    }

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_P(BarrierNotify, EventsDuringPassiveGrab)
{
    enum BarrierDeviceTestCombinations combination = GetParam();
    XORG_TESTCASE("Set up a pointer barrier.\n"
                  "Trigger a passive pointer grab\n"
                  "Move pointer against barrier.\n"
                  "Expect events\n");
    SCOPED_TRACE(enum_to_string(combination));
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

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, root, 20, 20, 20, 40, 0, ndevices, all_deviceids);
    XSync(dpy, False);

    ConfigureLateDevices(combination);

    /* if OE is true and mask is not set, but set on window → event */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &event_mask, 1);
    XIGrabButton(dpy, deviceid, 1, root, None, GrabModeAsync, GrabModeAsync, True, &no_mask, 1, &mods);
    (*target_dev)->PlayOne(EV_KEY, BTN_LEFT, 1, True);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    (*target_dev)->PlayOne(EV_KEY, BTN_LEFT, 0, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->event, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
        ASSERT_EQ(hit.ev->deviceid, deviceid);
        ASSERT_EQ(hit.ev->sourceid, sourceid);
        ASSERT_TRUE((hit.ev->flags & XIBarrierDeviceIsGrabbed));
    }
    XIUngrabButton(dpy, deviceid, 1, root, 1, &mods);

    /* if OE is false and mask is not set, but set on window → no event */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &event_mask, 1);
    XIGrabButton(dpy, deviceid, 1, root, None, GrabModeAsync, GrabModeAsync, False, &no_mask, 1, &mods);
    (*target_dev)->PlayOne(EV_KEY, BTN_LEFT, 1, True);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    (*target_dev)->PlayOne(EV_KEY, BTN_LEFT, 0, True);
    {
        XITEvent<XIDeviceEvent> press(dpy, GenericEvent, xi2_opcode, XI_ButtonPress);
        ASSERT_TRUE(press.ev);
    }

    ASSERT_FALSE(xorg::testing::XServer::WaitForEvent(dpy, 500));
    XIUngrabButton(dpy, deviceid, 1, root, 1, &mods);

    /* if OE is true and mask is set → event */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &no_mask, 1);
    XIGrabButton(dpy, deviceid, 1, root, None, GrabModeAsync, GrabModeAsync, True, &event_mask, 1, &mods);
    (*target_dev)->PlayOne(EV_KEY, BTN_LEFT, 1, True);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    (*target_dev)->PlayOne(EV_KEY, BTN_LEFT, 0, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->event, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
        ASSERT_EQ(hit.ev->deviceid, deviceid);
        ASSERT_EQ(hit.ev->sourceid, sourceid);
        ASSERT_TRUE((hit.ev->flags & XIBarrierDeviceIsGrabbed));
    }
    XIUngrabButton(dpy, deviceid, 1, root, 1, &mods);

    /* if OE is false and mask is set → event */
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XISelectEvents(dpy, root, &no_mask, 1);
    XIGrabButton(dpy, deviceid, 1, root, None, GrabModeAsync, GrabModeAsync, False, &event_mask, 1, &mods);
    (*target_dev)->PlayOne(EV_KEY, BTN_LEFT, 1, True);
    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);
    (*target_dev)->PlayOne(EV_KEY, BTN_LEFT, 0, True);
    ASSERT_TRUE(xorg::testing::XServer::WaitForEventOfType(dpy, GenericEvent, xi2_opcode, XI_BarrierHit, 500));
    {
        XITEvent<XIBarrierEvent> hit(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
        ASSERT_TRUE(hit.ev);
        ASSERT_EQ(hit.ev->event, root);
        ASSERT_EQ(hit.ev->root, root);
        ASSERT_EQ(hit.ev->barrier, barrier);
        ASSERT_EQ(hit.ev->root_x, 20);
        ASSERT_EQ(hit.ev->root_y, 30);
        ASSERT_EQ(hit.ev->deviceid, deviceid);
        ASSERT_EQ(hit.ev->sourceid, sourceid);
        ASSERT_TRUE((hit.ev->flags & XIBarrierDeviceIsGrabbed));
    }
    XIUngrabButton(dpy, deviceid, 1, root, 1, &mods);

    XFixesDestroyPointerBarrier(dpy, barrier);
}

TEST_P(BarrierNotify, BarrierRandREventsVertical)
{
    enum BarrierDeviceTestCombinations combination = GetParam();
    XORG_TESTCASE("Set up pointer barrier close to a screen.\n"
                  "Move the pointer against the barrier that the barrier blocks movement\n"
                  "Same movement must be restriced by the RandR dimensions"
                  "Ensure Barrier event root x/y are fully constrained");
    SCOPED_TRACE(enum_to_string(combination));
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

    SelectBarrierEvents(dpy, root);

    int w = DisplayWidth(dpy, DefaultScreen(dpy));
    int h = DisplayHeight(dpy, DefaultScreen(dpy));

    barrier = XFixesCreatePointerBarrier(dpy, root, w - 20, 0, w - 20, 4000, 0, ndevices, all_deviceids);

    ConfigureLateDevices(combination);
    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, w - 40 , h - 30);

    (*target_dev)->PlayOne(EV_REL, REL_X, 40, false);
    (*target_dev)->PlayOne(EV_REL, REL_Y, 100, true);

    XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
    ASSERT_EQ(barrier, event.ev->barrier);
    ASSERT_EQ(1U, event.ev->eventid);
    ASSERT_EQ(event.ev->root_x, w - 20 - 1);
    ASSERT_LT(event.ev->root_y, h);
    ASSERT_EQ(event.ev->deviceid, deviceid);
    ASSERT_EQ(event.ev->sourceid, sourceid);
}

TEST_P(BarrierNotify, ReceivesLeaveOnDestroyWhenInsideHitbox)
{
    enum BarrierDeviceTestCombinations combination = GetParam();
    XORG_TESTCASE("Set up pointer barrier.\n"
                  "Move the pointer into the barrier hitbox.\n"
                  "Destroy the barrier.\n"
                  "Ensure that we receive an event.");
    SCOPED_TRACE(enum_to_string(combination));

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    Window win = CreateWindow(dpy, root);

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, win, 20, 20, 20, 40, 0, ndevices, all_deviceids);
    XSync(dpy, False);

    SelectBarrierEvents(dpy, win);

    ConfigureLateDevices(combination);

    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XSync(dpy, False);

    (*target_dev)->PlayOne(EV_REL, REL_X, -40, True);

    /* Ensure we have a BarrierHit on our hands. */
    {
        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierHit);
    }

    XFixesDestroyPointerBarrier(dpy, barrier);

    {
        XITEvent<XIBarrierEvent> event(dpy, GenericEvent, xi2_opcode, XI_BarrierLeave);
        ASSERT_TRUE(event.ev);
        ASSERT_EQ(0, event.ev->dx);
        ASSERT_EQ(0, event.ev->dy);
        ASSERT_TRUE(event.ev->flags & XIBarrierPointerReleased);
        ASSERT_EQ(event.ev->deviceid, deviceid);
        ASSERT_EQ(event.ev->sourceid, 0);
    }

    ASSERT_TRUE(NoEventPending(dpy));
}

TEST_P(BarrierNotify, DoesntReceiveLeaveOnDestroyWhenOutsideHitbox)
{
    enum BarrierDeviceTestCombinations combination = GetParam();
    XORG_TESTCASE("Set up pointer barrier.\n"
                  "Move the pointer a bit, but don't make it touch the barrier.\n"
                  "Destroy the barrier.\n"
                  "Ensure that we didn't receive an event.");
    SCOPED_TRACE(enum_to_string(combination));

    ::Display *dpy = Display();
    Window root = DefaultRootWindow(dpy);
    Window win = CreateWindow(dpy, root);

    PointerBarrier barrier = XFixesCreatePointerBarrier(dpy, win, 20, 20, 20, 40, 0, ndevices, all_deviceids);
    XSync(dpy, False);

    SelectBarrierEvents(dpy, win);

    ConfigureLateDevices(combination);

    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XSync(dpy, False);

    /* Move the pointer, but don't hit the barrier. */
    (*target_dev)->PlayOne(EV_REL, REL_X, -5, True);

    XIWarpPointer(dpy, deviceid, None, root, 0, 0, 0, 0, 30, 30);
    XFixesDestroyPointerBarrier(dpy, barrier);
    ASSERT_FALSE(xorg::testing::XServer::WaitForEvent(dpy, 500));

    ASSERT_TRUE(NoEventPending(dpy));
}

INSTANTIATE_TEST_CASE_P(, BarrierNotify, ::testing::Values(NO_DEVICE_SPECIFICS,
                                                           VCP_ONLY,
                                                           TARGET_VCP,
                                                           TARGET_POINTER2,
                                                           TARGET_VCP_AND_ALL,
                                                           TARGET_POINTER2_AND_ALL,
                                                           LATE_SECOND_MD_VCP,
                                                           LATE_SECOND_MD_POINTER2));

#endif
