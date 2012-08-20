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

#endif

