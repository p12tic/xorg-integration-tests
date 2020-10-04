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

#include "device-emulated-interface.h"
#include <stdexcept>
#include <string>

void DeviceEmulatedInterface::AddDevice(xorg::testing::emulated::DeviceType device_type)
{
    devices.push_back(std::unique_ptr<xorg::testing::emulated::Device>(
        new xorg::testing::emulated::Device("xit-events-" + std::to_string(devices.size()),
                                            device_type)));
}

xorg::testing::emulated::Device& DeviceEmulatedInterface::Dev(unsigned i)
{
    if (i >= devices.size()) {
        throw std::runtime_error("Device does not exist");
    }
    return *devices[i];
}

void DeviceEmulatedInterface::WaitOpen()
{
    for (const auto& dev : devices) {
        dev->WaitOpen();
    }
}
