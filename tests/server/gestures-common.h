/*
 * Copyright © 2020 Povilas Kanapickas <povilas@radix.lt>
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

#ifndef XIT_SERVER_GESTURES_COMMON_H
#define  XIT_SERVER_GESTURES_COMMON_H

#include "xit-event.h"
#include "xit-server-input-test.h"
#include "device-emulated-interface.h"

#include <xorg/gtest/xorg-gtest.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XInput.h>

#if HAVE_XI24
class GestureTest : public XITServerInputTest,
                    public DeviceEmulatedInterface {
public:
    xorg::testing::emulated::Device& TouchPadDev() { return Dev(0); }
    xorg::testing::emulated::Device& TouchDev() { return Dev(1); }
    xorg::testing::emulated::Device& KeyboardDev() { return Dev(2); }

    bool IsPinch() const { return is_pinch; }
    void SetIsPinch(bool is) { is_pinch = is; }

    void SetUp() override
    {
        AddDevice(xorg::testing::emulated::DeviceType::POINTER_GESTURE);
        AddDevice(xorg::testing::emulated::DeviceType::TOUCH);
        AddDevice(xorg::testing::emulated::DeviceType::KEYBOARD);

        xi2_major_minimum = 2;
        xi2_minor_minimum = 4;

        XITServerInputTest::SetUp();
    }

    /**
     * Sets up an xorg.conf for a single evdev CoreKeyboard device based on
     * the evemu device. The input from GetParam() is used as XkbLayout.
     */
    void SetUpConfigAndLog() override
    {
        config.AddDefaultScreenWithDriver();
        config.AddInputSection("test", "--touchpad-device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n" +
                               TouchPadDev().GetOptions());
        config.AddInputSection("test", "--device--",
                               "Option \"CorePointer\" \"on\"\n"
                               "Option \"GrabDevice\" \"on\"\n" +
                               TouchDev().GetOptions());
        /* add default keyboard device to avoid server adding our device again */
        config.AddInputSection("test", "--kbd-device--",
                               "Option \"CoreKeyboard\" \"on\"\n" +
                               KeyboardDev().GetOptions());
        config.WriteConfig();
    }

    void StartServer() override
    {
        XITServerInputTest::StartServer();
        WaitOpen();
    }

    /**
     * Return a new synchronized client given our default server connection.
     */
    virtual ::Display* NewClient(int maj = 2, int min = 4)
    {
        ::Display *d = XOpenDisplay(server.GetDisplayString().c_str());
        if (!d)
            ADD_FAILURE() << "Failed to open display for new client.\n";
        XSynchronize(d, True);
        EnsureVersion(d, maj, min);
        return d;
    }

    void EnsureVersion(::Display* dpy, int major, int minor)
    {
        int got_major = major, got_minor = minor;
        if (major >= 2 && XIQueryVersion(dpy, &got_major, &got_minor) != Success)
            ADD_FAILURE() << "XIQueryVersion failed on new client.\n";
        if (got_major < major || got_minor < minor)
            ADD_FAILURE() << "The new client does not support requested input versions.\n";
    }

    enum GestureGrabType : unsigned {
        GrabPinchGestures,
        GrabSwipeGestures
    };

    void GrabDevice(::Display *dpy, int deviceid, Window win, int grab_mode,
                    const std::vector<int>& events)
    {
        EventMaskBuilder mask{VIRTUAL_CORE_POINTER_ID, events};
        ASSERT_EQ(Success, XIGrabDevice(dpy, deviceid,
                                        win, CurrentTime, None,
                                        grab_mode, GrabModeAsync,
                                        False, mask.GetMaskPtr()));
        XSync(dpy, False);
    }

    void GrabButton(::Display* dpy, Window win, int button, int grab_mode,
                    const std::vector<int>& events)
    {
        EventMaskBuilder mask{VIRTUAL_CORE_POINTER_ID, events};

        XIGrabModifiers mods = {};
        mods.modifiers = XIAnyModifier;
        ASSERT_EQ(Success, XIGrabButton(dpy, VIRTUAL_CORE_POINTER_ID, button, win, None,
                                        grab_mode, GrabModeAsync, False,
                                        mask.GetMaskPtr(), 1, &mods));
        XSync(dpy, False);
    }

    void GrabGestureBegin(::Display* dpy, Window win, int grab_mode, int other_grab_mode,
                          const std::vector<int>& events, bool is_pinch)
    {
        EventMaskBuilder mask{VIRTUAL_CORE_POINTER_ID, events};

        XIGrabModifiers mods = {};
        mods.modifiers = XIAnyModifier;
        if (is_pinch) {
            ASSERT_EQ(Success, XIGrabPinchGestureBegin(dpy, VIRTUAL_CORE_POINTER_ID, win, grab_mode,
                                                       other_grab_mode, False,
                                                       mask.GetMaskPtr(), 1, &mods));
        } else {
            ASSERT_EQ(Success, XIGrabSwipeGestureBegin(dpy, VIRTUAL_CORE_POINTER_ID, win, grab_mode,
                                                       other_grab_mode, False,
                                                       mask.GetMaskPtr(), 1, &mods));
        }
        XSync(dpy, False);
    }

    void GrabMatchingGestureBegin(::Display* dpy, Window win, int grab_mode, int other_grab_mode,
                                  const std::vector<int>& events)
    {
        GrabGestureBegin(dpy, win, grab_mode, other_grab_mode, events, IsPinch());
    }

    void GrabNonMatchingGestureBegin(::Display* dpy, Window win, int grab_mode, int other_grab_mode,
                                     const std::vector<int>& events)
    {
        GrabGestureBegin(dpy, win, grab_mode, other_grab_mode, events, !IsPinch());
    }

    void UngrabGestureBegin(::Display* dpy, Window win, bool is_pinch)
    {
        XIGrabModifiers mods = {};
        mods.modifiers = XIAnyModifier;
        if (is_pinch) {
            ASSERT_EQ(Success, XIUngrabPinchGestureBegin(dpy, VIRTUAL_CORE_POINTER_ID, win,
                                                         1, &mods));
        } else {
            ASSERT_EQ(Success, XIUngrabSwipeGestureBegin(dpy, VIRTUAL_CORE_POINTER_ID, win,
                                                         1, &mods));
        }
    }

    void UngrabMatchingGestureGrab(::Display* dpy, Window win)
    {
        UngrabGestureBegin(dpy, win, IsPinch());
    }

    void UngrabNonMatchingGestureGrab(::Display* dpy, Window win)
    {
        UngrabGestureBegin(dpy, win, !IsPinch());
    }

    int GetXIGestureBegin() const
    {
        return IsPinch() ? XI_GesturePinchBegin :  XI_GestureSwipeBegin;
    }
    int GetXIGestureUpdate() const
    {
        return IsPinch() ? XI_GesturePinchUpdate :  XI_GestureSwipeUpdate;
    }
    int GetXIGestureEnd() const
    {
        return IsPinch() ? XI_GesturePinchEnd :  XI_GestureSwipeEnd;
    }

    void GesturePlayBegin()
    {
        if (IsPinch()) {
            TouchPadDev().PlayGesturePinchBegin(3, 0, 0, 0, 0, 1.0, 0);
        } else {
            TouchPadDev().PlayGestureSwipeBegin(3, 0, 0, 0, 0);
        }
    }

    void GesturePlayUpdate()
    {
        if (IsPinch()) {
            TouchPadDev().PlayGesturePinchUpdate(3, 1, 1, 0, 0, 1.0, 0);
        } else {
            TouchPadDev().PlayGestureSwipeUpdate(3, 1, 1, 0, 0);
        }
    }

    void GesturePlayEnd()
    {
        if (IsPinch()) {
            TouchPadDev().PlayGesturePinchEnd(3, 1, 1, 1, 1, 1.0, 0);
        } else {
            TouchPadDev().PlayGestureSwipeEnd(3, 1, 1, 1, 1);
        }
    }

    void GesturePlayCancel()
    {
        if (IsPinch()) {
            TouchPadDev().PlayGesturePinchCancel(3, 1, 1, 1, 1, 1.0, 0);
        } else {
            TouchPadDev().PlayGestureSwipeCancel(3, 1, 1, 1, 1);
        }
    }

private:
    bool is_pinch = true;
};

class GestureTypesTest : public GestureTest,
                         public ::testing::WithParamInterface<int>
{
public:
    GestureTypesTest() {
        SetIsPinch(GetParam() == XI_GesturePinchBegin);
    }
};

class GestureRootChildWindowTest : public GestureTest,
                                   public ::testing::WithParamInterface<std::tuple<int, int, int>>
{
public:
    GestureRootChildWindowTest()
    {
        SetIsPinch(std::get<0>(GetParam()) == XI_GesturePinchBegin);
    }
    int Window1Depth() const { return std::get<1>(GetParam()); }
    int Window2Depth() const { return std::get<2>(GetParam()); }
};

#endif
#endif
