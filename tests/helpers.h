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
 * Kill the serer, remove the config file.
 */
void KillServer(::xorg::testing::XServer &server, XOrgConfig &config);

/**
 * Find the device in the input device list and return the number of
 * instances found.
 */
int FindInputDeviceByName(Display *dpy, const std::string& device_name);

#endif

