#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include "input-driver-test.h"

TEST_P(SimpleInputDriverTest, DriverDevice)
{
    std::ifstream in_file(server.GetLogFilePath().c_str());
    std::string line;
    std::string error_msg("Failed to load module");
    std::string param(GetParam());

    // grep for error message in the log file...
    error_msg +=  " \"" + param + "\"";
    if(in_file.is_open())
    {
        while (getline (in_file, line))
        {
            size_t found = line.find(error_msg);
            Bool error = (found != std::string::npos);
            ASSERT_FALSE (error) << "Module " << param << " failed to load" << std::endl << line <<  std::endl;
        }
    }
}

INSTANTIATE_TEST_CASE_P(, SimpleInputDriverTest,
        ::testing::Values("acecad", "aiptek", "elographics",
                          "fpit", "hyperpen",  "mutouch",
                          "penmount", "wacom", "synaptics",
                          "keyboard", "mouse", "vmmouse",
                          "evdev", "void"));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
