#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include "video-driver-test.h"

TEST_P(SimpleVideoDriverTest, DriverDevice)
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

INSTANTIATE_TEST_CASE_P(, SimpleVideoDriverTest,
                        ::testing::Values("ati", "cirrus", "dummy", "fbdev",
                                          "geode", "imx", "intel", "mga",
                                          "modesetting", "nv", "qxl",
                                          "r128", "rendition", "savage",
                                          "sis", "sisusb", "sunbw2",
                                          "sunleo", "tdfx", "vermilion",
                                          "vesa"));

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
