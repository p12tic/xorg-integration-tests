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
#include "device-emulated-interface.h"

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
                    public DeviceEmulatedInterface {
public:

    /**
     * Initializes a standard mouse device.
     */
    void SetUp() override {
        AddDevice(xorg::testing::emulated::DeviceType::POINTER);
        AddDevice(xorg::testing::emulated::DeviceType::KEYBOARD);
        BarrierBaseTest::SetUp();
    }

    void StartServer() override {
        BarrierBaseTest::StartServer();
        WaitOpen();
    }


    /**
     * Sets up an xorg.conf for a single emulated CoreKeyboard device based on
     * the emulated device. The input from GetParam() is used as XkbLayout.
     */
    void SetUpConfigAndLog() override {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("test", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n"
                               /* Disable pointer acceleration to allow for accurate
                                * pointer positions with EV_REL events... */
                               "Option \"AccelerationProfile\" \"-1\"\n" +
                               Dev(0).GetOptions());
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("test", "--device-kbd--",
                               "Option \"CoreKeyboard\" \"on\"\n" +
                               Dev(1).GetOptions());
        config.WriteConfig();
    }
};

class BarrierDevices : public BarrierBaseTest,
                       public DeviceEmulatedInterface {
public:
    int master_id_1;
    int master_id_2;

    void SetUp() override {
        AddDevice(xorg::testing::emulated::DeviceType::POINTER);
        AddDevice(xorg::testing::emulated::DeviceType::POINTER);
        AddDevice(xorg::testing::emulated::DeviceType::KEYBOARD);

        xi2_major_minimum = 2;
        xi2_minor_minimum = 3;

        BarrierBaseTest::SetUp();
        ConfigureDevices();
    }

    void StartServer() override {
        BarrierBaseTest::StartServer();
        WaitOpen();
    }

    void ConfigureDevices() {
        ::Display *dpy = Display();
        int device_id_2;

        XIAnyHierarchyChangeInfo change;
        change.add.type = XIAddMaster;
        change.add.name = (char *) "New Master";
        change.add.send_core = False;
        change.add.enable = True;

        master_id_1 = VIRTUAL_CORE_POINTER_ID;

        ASSERT_EQ(XIChangeHierarchy(dpy, &change, 1), Success) << "Couldn't add the new master device.";
        ASSERT_TRUE(xorg::testing::XServer::WaitForDevice(dpy, "New Master pointer")) << "Didn't get the new master pointer device.";
        ASSERT_TRUE(FindInputDeviceByName(dpy, "New Master pointer", &master_id_2)) << "Failed to find the new master pointer.";
        ASSERT_TRUE(FindInputDeviceByName(dpy, "--device1--", &device_id_2)) << "Failed to find device2.";

        change.attach.type = XIAttachSlave;
        change.attach.deviceid = device_id_2;
        change.attach.new_master = master_id_2;

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
        config.AddInputSection("test", "--device0--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n"
                               /* Disable pointer acceleration to allow for accurate
                                * pointer positions with relative events... */
                               "Option \"AccelerationProfile\" \"-1\"" +
                               Dev(0).GetOptions());
        config.AddInputSection("test", "--device1--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n"
                               /* Disable pointer acceleration to allow for accurate
                                * pointer positions with relative events... */
                               "Option \"AccelerationProfile\" \"-1\"\n" +
                               Dev(1).GetOptions());
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("test", "--device2--",
                               "Option \"CoreKeyboard\" \"on\"\n" +
                               Dev(2).GetOptions());
        config.WriteConfig();
    }
};

#endif
