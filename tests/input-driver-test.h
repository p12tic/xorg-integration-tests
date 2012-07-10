#ifndef _INPUT_DRIVER_TEST_H_
#define _INPUT_DRIVER_TEST_H_

#include <xorg/gtest/xorg-gtest.h>

/**
 * A test fixture for testing some input drivers
 *
 */
class InputDriverTest : public xorg::testing::Test,
                        public ::testing::EmptyTestEventListener,
                        public ::testing::WithParamInterface<std::string> {
protected:
    /**
     * Write a temporary config file prefixed with the given argument,
     * resulting in a file name of /tmp/prefix.conf.
     *
     * On test success, the config file will be deleted.
     *
     * @param prefix The config file name prefix
     */
    virtual void WriteConfig(const std::string &prefix);


    /**
     * Add a Section InputDevice to the config file.
     *
     * @param identifier Device identifier, or "--device--" if left out
     * @param driver The driver used for this device
     * @param options "Option foo " directives.
     */
    virtual void AddInputSection(std::string driver,
                                 std::string identifier = "--device--",
                                 std::string options = "");

    /**
     * Sets ::failed to true if this test failed.
     */
    virtual void OnTestPartResult(const ::testing::TestPartResult &test_part_result);

    virtual void StartServer();
    virtual void SetUp();
    virtual void TearDown();

    int xi2_opcode;
    std::string config_file;
    std::string log_file;
    xorg::testing::XServer server;
    bool failed;

};

#endif
