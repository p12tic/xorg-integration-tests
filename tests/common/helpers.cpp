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
    XErrorEvent *error;
    XErrorHandler prev_error_handler;
} trap_state = { false, NULL, NULL };

static int
ErrorHandler(Display *dpy,
             XErrorEvent *error)
{
    if (!trap_state.is_trapping)
        ADD_FAILURE() << "Error trap received error while not trapping. WTF?";

    trap_state.error = error;
    return 0;
}

void SetErrorTrap(Display *dpy) {
    if (trap_state.is_trapping)
        ADD_FAILURE() << "SetErrorTrap() called while already trapping.";

    XSync(dpy, False);
    trap_state.prev_error_handler = XSetErrorHandler(ErrorHandler);
    trap_state.is_trapping = true;
}

XErrorEvent * ReleaseErrorTrap(Display *dpy) {
    if (!trap_state.is_trapping)
        ADD_FAILURE() << "ReleaseErrorTrap() called while not trapping.";

    XSync(dpy, False);
    XSetErrorHandler(trap_state.prev_error_handler);
    trap_state.prev_error_handler = NULL;

    XErrorEvent *error = trap_state.error;
    trap_state.error = NULL;
    trap_state.is_trapping = false;
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
