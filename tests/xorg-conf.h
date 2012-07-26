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
     * @param reference_from_layout True to reference the device from the
     * server layout (default)
     */
    virtual void AddInputSection(const std::string &driver,
                                 const std::string &identifier = "--device--",
                                 const std::string &options = "",
                                 bool reference_from_layout = true);

    void AddDefaultScreenWithDriver(const std::string &driver = "dummy",
                                    const std::string &identifier = "dummy",
                                    const std::string &options = "");

    /**
     * Enable or disable AutoAddDevices
     * @param enabled True to enable AutoAddDevices, False to disable
     */
    void SetAutoAddDevices(bool enabled);

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
    std::vector<std::string> input_devices;
    std::string default_device;
    bool auto_add_devices;
};

#endif
