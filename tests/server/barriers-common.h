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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _BARRIERS_COMMON_H_
#define _BARRIERS_COMMON_H_

#include <xorg/gtest/xorg-gtest.h>

#include "xit-server-input-test.h"
#include "device-interface.h"

class BarrierBaseTest : public XITServerInputTest {
public:
    int xfixes_opcode;
    int xfixes_event_base;
    int xfixes_error_base;

    virtual void SetUp(void) {
        XITServerInputTest::SetUp();
        RequireXFixes(5, 0);
    }

    virtual void RequireXFixes(int major_req, int minor_req) {
        if (!XQueryExtension (Display(), XFIXES_NAME,
                              &xfixes_opcode,
                              &xfixes_event_base,
                              &xfixes_error_base)) {
            ADD_FAILURE () << "Need XFixes.\n";
        }

        int major, minor;
        if (!XFixesQueryVersion (Display(), &major, &minor) ||
            major * 100 + minor < major_req * 100 + minor_req)
            ADD_FAILURE () << "Need XFixes " << major_req << "." << minor_req << ", got " << major << "." << minor;

    }

};

class BarrierTest : public BarrierBaseTest,
                    public DeviceInterface {
public:

    /**
     * Initializes a standard mouse device.
     */
    virtual void SetUp() {
        SetDevice("mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc");
        BarrierBaseTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n"
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

class BarrierDevices : public BarrierBaseTest {
public:
    std::auto_ptr<xorg::testing::evemu::Device> dev1;
    std::auto_ptr<xorg::testing::evemu::Device> dev2;

    int master_id_1;
    int master_id_2;

    virtual void SetUp() {
        SetUpDevices();
        BarrierBaseTest::SetUp();
        RegisterXI2(2, 3);

        ConfigureDevices();
    }

    virtual void SetUpDevices() {
        dev1 = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc"
                ));
        dev2 = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(
                    RECORDINGS_DIR "mice/PIXART-USB-OPTICAL-MOUSE-HWHEEL.desc"
                ));
    }

    void ConfigureDevices() {
        ::Display *dpy = Display();
        int device_id_2;

        XIAnyHierarchyChangeInfo change;
        change.add = (XIAddMasterInfo) {
            .type = XIAddMaster,
            .name = (char *) "New Master",
            .send_core = False,
            .enable = True
        };

        master_id_1 = VIRTUAL_CORE_POINTER_ID;

        ASSERT_EQ(XIChangeHierarchy(dpy, &change, 1), Success) << "Couldn't add the new master device.";
        ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(dpy, "New Master pointer")) << "Didn't get the new master pointer device.";
        ASSERT_TRUE(FindInputDeviceByName(dpy, "New Master pointer", &master_id_2)) << "Failed to find the new master pointer.";
        ASSERT_TRUE(FindInputDeviceByName(dpy, "--device2--", &device_id_2)) << "Failed to find device2.";

        change.attach = (XIAttachSlaveInfo) {
            .type = XIAttachSlave,
            .deviceid = device_id_2,
            .new_master = master_id_2,
        };
        ASSERT_EQ(XIChangeHierarchy(dpy, &change, 1), Success) << "Couldn't attach device2 to the new master pointer.";
    }

#if HAVE_XI23
    virtual void SelectBarrierEvents(::Display *dpy, Window win) {
        XIEventMask mask;
        mask.deviceid = XIAllMasterDevices;
        mask.mask_len = XIMaskLen(XI_LASTEVENT);
        mask.mask = new unsigned char[mask.mask_len]();
        XISetMask(mask.mask, XI_BarrierHit);
        XISetMask(mask.mask, XI_BarrierLeave);
        XISelectEvents(dpy, win, &mask, 1);
        delete[] mask.mask;
        XSync(dpy, False);
    }
#endif

    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device1--",
                               "Option \"CorePointer\" \"on\""
                               "Option \"GrabDevice\" \"on\"\n"
                               "Option \"AccelerationProfile\" \"-1\""
                               "Option \"Device\" \"" + dev1->GetDeviceNode() + "\"");

        config.AddInputSection("evdev", "--device2--",
                               "Option \"CorePointer\" \"on\""
                               "Option \"GrabDevice\" \"on\"\n"
                               "Option \"AccelerationProfile\" \"-1\""
                               "Option \"Device\" \"" + dev2->GetDeviceNode() + "\"");

        config.AddInputSection("kbd", "kbd-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }
};

#endif
