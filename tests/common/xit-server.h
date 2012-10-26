#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _XIT_SERVER_H
#define _XIT_SERVER_H

#include <xorg/gtest/xorg-gtest.h>

/**
 * Wrapper around the xorg-gtest XServer class that provides default setup
 * routines for the XIT suite
 */
class XITServer : public xorg::testing::XServer {
public:
    XITServer();
};

#endif
