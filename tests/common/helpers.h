#if HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef _HELPERS_H_
#define _HELPERS_H_

#include <string>
#include <xorg/gtest/xorg-gtest.h>
#include "xorg-conf.h"

#include "xit-server.h"

#define VIRTUAL_CORE_POINTER_ID 2
#define VIRTUAL_CORE_KEYBOARD_ID 3

/**
 * Find the device in the input device list and return the number of
 * instances found.
 *
 * @param [out] deviceid If not NULL, deviceid is set to the id of the
 *                       device. If multiple devices with the same name
 *                       exist, the behaviour is undefined.
 */
int FindInputDeviceByName(Display *dpy, const std::string& device_name, int *deviceid = NULL);

/**
 * Initialized XRandR support by selecting appropriate events to be
 * notified of RandR related changes.
 *
 * Returns TRUE if the display support XRandR >= 1.2
 *
 * @param [out] event_base If not NULL, the value is set to the first event
 *                         code.
 * @param [out] error_base If not NULL, the value is set to the first error
 *                         code.
 */
Bool InitRandRSupport (Display *dpy, int *event_base, int *error_base);

/**
 * Returns the number of connected monitors
 */
int GetNMonitors (Display *dpy);

/**
 * Returns the geometry of the given monitor specified by its number.
 *
 * @param [out] x      If not NULL, the value is set to the X position of the 
 *                     monitor.
 * @param [out] y      If not NULL, the value is set to the Y position of the 
 *                     monitor.
 * @param [out] width  If not NULL, the value is set to the width of the 
 *                     monitor.
 * @param [out] height If not NULL, the value is set to the height of the 
 *                     monitor.
 */
void GetMonitorGeometry (Display *dpy, int monitor, int *x, int *y, int *width, int *height);

/**
 * Enable or disable the input device
 *
 * @param [in] dpy      The display connection
 * @param [in] deviceid The numerical device ID for this device
 * @param [in] enabled  true to enable the device, false to disable
 */
void DeviceSetEnabled(Display *dpy, int deviceid, bool enabled);

/**
 * Set an error trap. This is designed to be wound tightly
 * around an Xlib call that you're expecting an error for:
 *
 *   // Ensure that bad devices raise an error.
 *   SetErrorTrap(dpy);
 *   XIQueryDevice(dpy, 0xBAD, &ndevice);
 *   XErrorEvent *err = ReleaseErrorTrap(dpy);
 *   ASSERT_EQ(BadDevice, err->code);
 *
 * Note that if multiple errors come in, the second will
 * overwrite the first. These functions are also meant to
 * tightly wrap Xlib calls, and don't really do any state, so
 * calling SetErrorTrap() twice, and then ReleaseErrorTrap() once
 * will remove the error trap entirely.
 *
 * @param [in] dpy The display to trap errors for.
 */
void SetErrorTrap(Display *dpy);

/**
 * Release an error trap. See the documentation of SetErrorTrap()
 * for more details.
 *
 * @param [in] dpy The display to release error trapping for.
 *
 * @return An XErrorEvent that was trapped.
 */
const XErrorEvent* ReleaseErrorTrap(Display *dpy);

#define ASSERT_ERROR(err, code)                                         \
{                                                                       \
    const XErrorEvent* e = (err);                                       \
    ASSERT_TRUE(e != NULL) << ("Expected " #code);                      \
    ASSERT_EQ(code, (int)e->error_code) << ("Expected " #code);         \
}

#define ASSERT_NO_ERROR(err)                            \
    ASSERT_TRUE(err == NULL) << "Expected no error"


/**
 * Does not much for normal device IDs, but returns
 * XIAllDevices/XIAllMasterDevices where applicable.
 *
 * @return string representing this deviceID.
 */
std::string DeviceIDToString(int deviceid);

/**
 * Returns current root coordinates for the VCP.
 *
 * @return true if the pointer is on the same screen as the default root
 * window, false otherwise
 */
Bool QueryPointerPosition(Display *dpy, double *root_x, double *root_y, int deviceid = VIRTUAL_CORE_POINTER_ID);

/**
 * Warps the VCP to the position of the default root window.
 */
void WarpPointer(Display *dpy, int x, int y);

/**
 * Create and map a window below the given parent. Default is a full-screen window.
 */
Window CreateWindow(Display *dpy, Window parent = None,
                    int x = 0, int y = 0,
                    int width = -1, int height = -1);

/**
 * Check if there are any events on the wire and if so, print information
 * about the first one.
 *
 * Typical usage: ASSERT_TRUE(NoEventPending(dpy))
 *
 * @return true if the event queue is empty, false otherwise
 */
bool NoEventPending(Display *dpy);

/**
 * Open the file at path and search it for the substring given. Search is
 * per-line and for location of the substring within that line.
 *
 * @return true if the file at path contains the substring in any line, or
 * false otherwise.
 */
bool SearchFileForString(const std::string &path, const std::string &substring);
#endif

