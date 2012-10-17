#if HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef _HELPERS_H_
#define _HELPERS_H_

#include <string>
#include <xorg/gtest/xorg-gtest.h>
#include "xorg-conf.h"

/**
 * Start the server after writing the config, with the prefix given.
 */
void StartServer(std::string prefix, ::xorg::testing::XServer &server, XOrgConfig &config);

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
#endif

