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

        int major, minor;
        if (!XFixesQueryVersion (Display(), &major, &minor) ||
                major < 5)
            ADD_FAILURE () << "Need XFixes 5 or later\n";
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

class BarrierDevices : public XITServerInputTest {
public:
    std::auto_ptr<xorg::testing::evemu::Device> dev1;
    std::auto_ptr<xorg::testing::evemu::Device> dev2;

    int master_id_1;
    int master_id_2;

    virtual void SetUp() {
        SetUpDevices();
        XITServerInputTest::SetUp();
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

    virtual void SetUpConfigAndLog() {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("evdev", "--device1--",
                               "Option \"CorePointer\" \"on\""
                               "Option \"AccelerationProfile\" \"-1\""
                               "Option \"Device\" \"" + dev1->GetDeviceNode() + "\"");

        config.AddInputSection("evdev", "--device2--",
                               "Option \"CorePointer\" \"on\""
                               "Option \"AccelerationProfile\" \"-1\""
                               "Option \"Device\" \"" + dev2->GetDeviceNode() + "\"");

        config.AddInputSection("kbd", "kbd-device",
                               "Option \"CoreKeyboard\" \"on\"\n");
        config.WriteConfig();
    }
};

#endif
