#ifndef _XORGCONFIG_H_
#define _XORGCONFIG_H_

#include <string>
#include <vector>

class XOrgConfig {
public:
    /**
     * Create a new configuration file. This doesn't write to a file until
     * ::WriteConfig is called.
     *
     * @param path The future default path to write to, or a default path if
     * none is given
     */
    XOrgConfig(const std::string path = "/tmp/xorg.conf");

    /**
     * Write the current config to the path given.
     * @param path The target path to write to, or the stored path in the
     * object
     */
    virtual void WriteConfig(const std::string &path = "");

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
     * @return the path of the config file.
     */
    const std::string& GetPath();

    /**
     * @param the new path of the config file.
     */
    void SetPath(const std::string &path);

protected:
    std::string config_file;
    std::vector<std::string> sections;
};

#endif
