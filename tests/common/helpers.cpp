#if HAVE_CONFIG_H
#include <config.h>
#endif
#include "helpers.h"

#include <sstream>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>

int FindInputDeviceByName(Display *dpy, const std::string &device_name, int *deviceid)
{
    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(dpy, XIAllDevices, &ndevices);

    int found = 0;
    while(ndevices--) {
        if (strcmp(info[ndevices].name, device_name.c_str()) == 0) {
            if (deviceid)
                *deviceid = info[ndevices].deviceid;
            found++;
        }
    }

    if (found > 1) {
        SCOPED_TRACE("More than one device named '" + device_name +
                     "' found.\nThis may cause some tests to fail.\n");
    }

    XIFreeDeviceInfo(info);

    return found;
}

Bool InitRandRSupport (Display *dpy, int *event_base, int *error_base)
{
    int ignore1, ignore2;
    int major, minor;

    if (event_base == NULL)
        event_base = &ignore1;

    if (error_base == NULL)
        error_base = &ignore2;

    if (!XRRQueryExtension (dpy, event_base, error_base))
        return False;

    XRRQueryVersion (dpy, &major, &minor);
    if (major <= 1 && minor < 2)
        return False;

    XSelectInput (dpy, DefaultRootWindow(dpy), StructureNotifyMask);
    XRRSelectInput (dpy,
                    DefaultRootWindow(dpy),
                    RRScreenChangeNotifyMask	|
                    RRCrtcChangeNotifyMask	|
                    RROutputPropertyNotifyMask);

    return True;
}

int GetNMonitors (Display *dpy)
{
    XRRScreenResources *resources;
    int i, n_active_outputs = 0;

    resources = XRRGetScreenResourcesCurrent (dpy, DefaultRootWindow(dpy));
    for (i = 0; i < resources->noutput; ++i) {
        XRROutputInfo *output = XRRGetOutputInfo (dpy, resources, resources->outputs[i]);

        if ((output->connection != RR_Disconnected) && output->crtc)
            n_active_outputs++;

        XRRFreeOutputInfo (output);
    }

    XRRFreeScreenResources (resources);

    return n_active_outputs;
}

void GetMonitorGeometry (Display *dpy, int monitor, int *x, int *y, int *width, int *height)
{
    XRRScreenResources *resources;
    int i, n_active_outputs = 0;

    resources = XRRGetScreenResourcesCurrent (dpy, DefaultRootWindow(dpy));
    for (i = 0; i < resources->noutput; ++i) {
        XRROutputInfo *output = XRRGetOutputInfo (dpy, resources, resources->outputs[i]);

        if ((output->connection == RR_Disconnected) || !output->crtc) {
            XRRFreeOutputInfo (output);
            continue;
        }

        if (n_active_outputs == monitor) {
            XRRCrtcInfo *crtc = XRRGetCrtcInfo (dpy, resources, output->crtc);

            if (x)
                *x = crtc->x;
            if (y)
                *y = crtc->y;
            if (width)
                *width = crtc->width;
            if (height)
                *height = crtc->height;

            XRRFreeCrtcInfo (crtc);
            break;
        }
        n_active_outputs++;
    }

    XRRFreeScreenResources (resources);
}

void DeviceSetEnabled(Display *dpy, int deviceid, bool enabled)
{
    Atom enabled_prop = XInternAtom(dpy, "Device Enabled", True);
    ASSERT_NE(enabled_prop, (Atom)None);
    unsigned char data = enabled ? 1 : 0;
    XIChangeProperty(dpy, deviceid, enabled_prop, XA_INTEGER, 8,
                     PropModeReplace, &data, 1);
    XFlush(dpy);
}

/* Basic error trapping */
static struct {
    bool is_trapping;
    bool have_error;
    XErrorEvent error;
    XErrorHandler prev_error_handler;
} trap_state;

static int
ErrorHandler(Display *dpy,
             XErrorEvent *error)
{
    if (!trap_state.is_trapping)
        ADD_FAILURE() << "Error trap received error while not trapping. WTF?";

    trap_state.have_error = true;
    trap_state.error = *error;
    return 0;
}

void SetErrorTrap(Display *dpy) {
    if (trap_state.is_trapping)
        ADD_FAILURE() << "SetErrorTrap() called while already trapping.";

    XSync(dpy, False);
    trap_state.prev_error_handler = XSetErrorHandler(ErrorHandler);
    trap_state.is_trapping = true;
}

const XErrorEvent* ReleaseErrorTrap(Display *dpy) {
    if (!trap_state.is_trapping)
        ADD_FAILURE() << "ReleaseErrorTrap() called while not trapping.";

    XSync(dpy, False);
    XSetErrorHandler(trap_state.prev_error_handler);
    trap_state.prev_error_handler = NULL;

    const XErrorEvent *error = trap_state.have_error ? &trap_state.error : NULL;
    trap_state.is_trapping = false;
    trap_state.have_error = false;
    return error;
}

std::string DeviceIDToString(int deviceid) {
    switch (deviceid) {
        case XIAllDevices:
            return "XIAllDevices";
        case XIAllMasterDevices:
            return "XIAllMasterDevices";
        default:
            return static_cast<std::ostringstream*>(&(std::ostringstream() << deviceid) )->str();
    }
}

Bool QueryPointerPosition(Display *dpy, double *root_x, double *root_y, int deviceid)
{
    Window root, child;
    double win_x, win_y;
    XIButtonState buttons;
    XIModifierState mods;
    XIGroupState group;

    return XIQueryPointer (dpy, deviceid,
                           DefaultRootWindow (dpy),
                           &root, &child,
                           root_x, root_y,
                           &win_x, &win_y,
                           &buttons, &mods, &group);
}

void WarpPointer(Display *dpy, int x, int y)
{
    XIWarpPointer(dpy, VIRTUAL_CORE_POINTER_ID, None, DefaultRootWindow(dpy),
                  0, 0, 0, 0, x, y);
    XSync(dpy, False);
}

Window CreateWindow(Display *dpy, Window parent,
                    int x, int y,
                    int width, int height)
{
    Window win;

    if (width == -1)
        width = DisplayWidth(dpy, DefaultScreen(dpy));
    if (height == -1)
        DisplayHeight(dpy, DefaultScreen(dpy));
    if (parent == None)
        parent = DefaultRootWindow(dpy);

    win = XCreateSimpleWindow(dpy, parent, x, y, width, height, 0, 0, 0);
    XSelectInput(dpy, win, StructureNotifyMask);
    XMapWindow(dpy, win);
    if (xorg::testing::XServer::WaitForEventOfType(dpy, MapNotify, -1, -1)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
    } else {
        ADD_FAILURE() << "Failed waiting for Exposure";
    }
    XSelectInput(dpy, win, 0);
    XSync(dpy, False);
    return win;
}

bool NoEventPending(::Display *dpy)
{
    XSync(dpy, False);
    if (XPending(dpy)) {
        XEvent ev;
        XPeekEvent(dpy, &ev);
        std::stringstream ss;
        EXPECT_EQ(XPending(dpy), 0) << "Event type " << ev.type << " (extension " <<
            ev.xcookie.extension << " evtype " << ev.xcookie.evtype << ")";
    }

    return XPending(dpy) == 0;
}
