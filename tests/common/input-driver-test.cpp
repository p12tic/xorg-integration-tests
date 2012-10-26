#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdexcept>
#include <xorg/gtest/xorg-gtest.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"
#include "helpers.h"

int InputDriverTest::RegisterXI2(int major, int minor)
{
    int event_start;
    int error_start;

    if (!XQueryExtension(Display(), "XInputExtension", &xi2_opcode,
                         &event_start, &error_start))
        ADD_FAILURE() << "XQueryExtension returned FALSE";

    int major_return = major;
    int minor_return = minor;
    if (XIQueryVersion(Display(), &major_return, &minor_return) != Success)
        ADD_FAILURE() << "XIQueryVersion failed";
    if (major_return != major)
       ADD_FAILURE() << "XIQueryVersion returned invalid major";

    return minor_return;
}

void InputDriverTest::StartServer() {
    XITServerTest::StartServer();
    RegisterXI2();
}

void InputDriverTest::SetUpConfigAndLog() {
    InitDefaultLogFiles(server, &config);
}

void InputDriverTest::SetUp() {
    SetUpConfigAndLog();
    XITServerTest::SetUp();
}
