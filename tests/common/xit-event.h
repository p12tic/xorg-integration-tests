#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>

#ifndef _XIT_EVENT_H_
#define _XIT_EVENT_H_

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
    if (!XPending(dpy) && !xorg::testing::XServer::WaitForEventOfType(dpy, type, opcode, evtype)) {
        ADD_FAILURE() << "Event not received before timeout";
        ev = NULL;
        return;
    }

    XNextEvent(dpy, &e);

    if (e.type != type)
        ADD_FAILURE() << "Mismatching type: " << e.type << " (expected " << type << ")";

    if (type != GenericEvent)
        ev = reinterpret_cast<EventType*>(&e);
    else {
        XGetEventData(dpy, &e.xcookie);
        if (e.xcookie.evtype != evtype)
            ADD_FAILURE() << "Mismatching XGE evtypes: " <<
                e.xcookie.evtype << " (expected " << evtype << ")";
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
