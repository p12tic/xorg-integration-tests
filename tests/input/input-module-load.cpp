#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include "xit-server-input-test.h"

/* No implementation, class only exists for test naming */
class InputModuleLoadTest : public SimpleInputDriverTest {};

TEST_P(InputModuleLoadTest, CheckForLoadFailure)
{
    std::string error_msg("Failed to load module");
    std::string param(GetParam());

    error_msg +=  " \"" + param + "\"";
    ASSERT_FALSE(SearchFileForString(server.GetLogFilePath(), error_msg))
             << "Module " << param << " failed to load" << std::endl;
}

INSTANTIATE_TEST_CASE_P(, InputModuleLoadTest,
        ::testing::Values("acecad", "aiptek", "elographics",
                          "fpit", "hyperpen",  "mutouch",
                          "penmount", "wacom", "synaptics",
                          "keyboard", "mouse", "vmmouse",
                          "evdev", "void"));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
