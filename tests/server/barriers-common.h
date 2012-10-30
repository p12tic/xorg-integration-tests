#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _BARRIERS_COMMON_H_
#define _BARRIERS_COMMON_H_

#include <xorg/gtest/xorg-gtest.h>

#include "xit-server-input-test.h"
#include "device-interface.h"

class BarrierTest : public XITServerInputTest,
                    public DeviceInterface {
public:
    int xfixes_opcode;
    int xfixes_event_base;
    int xfixes_error_base;

    /**
     * Initializes a standard mouse device.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        XITServerInputTest::SetUp();

        if (!XQueryExtension (Display(), XFIXES_NAME,
                              &xfixes_opcode,
                              &xfixes_event_base,
                              &xfixes_error_base)) {
            ADD_FAILURE () << "Need XFixes.\n";
        }
    }

    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               /* Disable pointer acceleration to allow for accurate
                                * pointer positions with EV_REL events... */
                               "Option \"AccelerationProfile\" \"-1\""
                               "Option \"Device\" \"" + dev->GetDeviceNode() + "\"");
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("kbd", "kbd-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }
};

#endif
