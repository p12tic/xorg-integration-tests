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

#ifndef _XIT_EVENT_H_
#define _XIT_EVENT_H_

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <xorg/gtest/xorg-gtest.h>

/**
 * Usage:
 *   ASSERT_EVENT(XIDeviceEvent, myevent, dpy, GenericEvent, xi2_opcode, XI_ButtonPress);
 *   ASSERT_EQ(myevent->detail, 1U);
 *
 * or for core events:
 *   ASSERT_EVENT(XMotionEvent, myevent, dpy, MotionNotify);
 *   ASSERT_EQ(myevent->detail, 1U);
 */
// We use empty "do {} while (false)" statement at the end to permit the use of the macros as
// statements ending with semicolons. This fixes empty statement warnings throughout the codebase.
#define ASSERT_EVENT(_type, _name, ...) \
        XITEvent<_type> _name ## _xit_event(__VA_ARGS__); \
        _type* _name = _name ## _xit_event.ev; \
        ASSERT_TRUE(_name); do {} while (false)

#define EXPECT_EVENT(_type, _name, ...) \
        XITEvent<_type> _name ## _xit_event(__VA_ARGS__); \
        _type* _name = _name ## _xit_event.ev; \
        EXPECT_TRUE(_name); do {} while (false)
/**
 * Template class to work around the Xlib cookie API.
 * Create a single EW object that pulls down then next event from the wire,
 * gets the cookie data (if needed) and frees the cookie data once the
 * object goes out of scope.
 *
 * Works for legacy events as well.
 *
 * Usage:
 * XITEvent<XIDeviceEvent> my_event(dpy, GenericEvent, xi2_opcode, * XI_TouchEnd);
 * ASSERT_TRUE(my_event.ev);
 * ASSERT_EQ(my_event.ev->detail, touchid);
 * etc.
 */
template <typename EventType>
class XITEvent {
private:
    XEvent e;

    /* I'm too lazy for that right now. */
    XITEvent(const XITEvent&);
    XITEvent& operator=(const XITEvent &);

public:
    EventType *ev;

    XITEvent(::Display *dpy, int type, int opcode = -1, int evtype = -1);
    ~XITEvent();


    template <typename T>
    friend std::ostream& operator<< (std::ostream &stream, const XITEvent<T> &ev);
};


template<typename EventType>
XITEvent<EventType>::XITEvent(::Display *dpy, int type, int opcode, int evtype)
{
    if (!xorg::testing::XServer::WaitForEvent(dpy)) {
        ADD_FAILURE() << "Event not received before timeout";
        ev = NULL;
        return;
    }

    XNextEvent(dpy, &e);

    if (e.type != type) {
        std::stringstream msg;
        msg << "Mismatching type: " << e.type;
        if (e.type == GenericEvent)
            msg << " extension: " << ((XGenericEvent*)&e)->extension << " evtype: " << ((XGenericEvent*)&e)->evtype;
        msg << " (expected " << type << ")";
        ADD_FAILURE() << msg.str();
        ev = NULL;
        return;
    }

    if (type != GenericEvent)
        ev = reinterpret_cast<EventType*>(&e);
    else {
        XGetEventData(dpy, &e.xcookie);
        if (e.xcookie.evtype != evtype) {
            ADD_FAILURE() << "Mismatching XGE evtypes: " <<
                e.xcookie.evtype << " (expected " << evtype << ")";
            XFreeEventData(dpy, &e.xcookie);
            ev = NULL;
            return;
        }
        ev = reinterpret_cast<EventType*>(e.xcookie.data);
    }
}

template<typename EventType>
XITEvent<EventType>::~XITEvent() {
    if (e.type == GenericEvent)
        XFreeEventData(e.xcookie.display, &e.xcookie);
}

template <typename EventType>
std::ostream& operator<< (std::ostream &stream, const XITEvent<EventType> &event)
{
    stream << "Event type " << event.e.type;
    if (event.e.type == GenericEvent)
        stream << " extension " << event.ev->extension << " evtype " << event.ev->evtype;
    return stream;
}

#endif /* _XIT_EVENT_H_ */
