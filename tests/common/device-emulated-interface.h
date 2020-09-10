/*
 * Copyright (C) 2020 Povilas Kanapickas <povilas@radix.lt>
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

#ifndef DEVICE_EMULATED_INTERFACE_H_
#define DEVICE_EMULATED_INTERFACE_H_

#include <xorg/gtest/xorg-gtest.h>

/**
 * A test fixture for testing X server input processing.
 *
 * Do not instanciate this class directly, subclass it from the test case
 * instead.
 */
class DeviceEmulatedInterface {
protected:
    std::vector<std::unique_ptr<xorg::testing::emulated::Device>> devices;

    xorg::testing::emulated::Device& Dev(unsigned i);

    void WaitOpen();

    void AddDevice(xorg::testing::emulated::DeviceType device_type);
};

#endif
