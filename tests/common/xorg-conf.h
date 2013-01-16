#if HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef _XORGCONFIG_H_
#define _XORGCONFIG_H_

#include <string>
#include <vector>

/**
 * xorg.conf writer class. This class abstracts the more common options to
 * write a xorg.conf.
 *
 * @code
 * XOrgConfig config;
 * config.AddDefaultScreenWithDriver();
 * config.AddInputSection("evdev");
 * config.WriteConfig("/tmp/my.conf");
 * ....
 * config.RemoveConfig();
 *
 * @endcode
 */
class XOrgConfig {
public:
    /**
     * Create a new configuration file. A configuration file is not written
     * to the file system until WriteConfig() is called.
     *
     * @param path The future default path to write to, or a default path if
     * none is given
     */
    XOrgConfig(const std::string& path = "");

    /**
     * Write the current config to the path given. If no path is specified,
     * the path used is the one either set in XOrgConfig() or by SetPath().
     *
     * @param path The target path to write to, or the stored path in the
     * object
     */
    virtual void WriteConfig(const std::string &path = "");

    /**
     * Remove the config previously written (if any).
     *
     * This must be called explicitly, the destructor does not remove the
     * the config file. This is intentional, a test that fails and thus
     * leaves the scope should leave the config file around that cause the
     * test failure.
     */
    virtual void RemoveConfig();

    /**
     * Add a Section InputDevice to the config file. This section is
     * optionally referenced from the ServerLayout written by this section,
     * and in the form of:
     *
     * @code
     * Section "InputDevice"
     *    Identifier "--device--"
     *    Driver "driver"
     *    ...
     * EndSection
     * @endcode
     *
     * Options may be specified by the caller and must be a single string.
     * It is recommened that multiple options are separated by line-breaks
     * to improve the readability of the config file.
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

    /**
     * Add a Section InputClass to the config file, in the form of:
     * @code
     * Section "InputClass"
     *    Identifier "<identifier>"
     *    ... match conditions ...
     *    ... options ...
     * @endcode
     *
     * @param identifier The identifier for this input class
     * @param matches Single string containing all match conditions
     * @param options Single string containing all options to be set
     */
    virtual void AddInputClass(const std::string &identifier,
                               const std::string &matches,
                               const std::string &options);


    /**
     * Add a Screen and Device section to the config file. The resulting
     * config file will look like this:
     *
     * @code
     * Section "Device"
     *     Identifier "dummy"
     *     Driver "dummy"
     *     ...
     * EndSection
     *
     * Section "Screen"
     *     Identifier "dummy"
     *     Device "dummy"
     * EndSection
     * @endcode
     *
     * Options may be specified by the caller and must be a single string.
     * It is recommened that multiple options are separated by line-breaks
     * to improve the readability of the config file.
     */
    void AddDefaultScreenWithDriver(const std::string &driver = "dummy",
                                    const std::string &identifier = "dummy",
                                    const std::string &options = "");

    /**
     * Enable or disable AutoAddDevices.
     *
     * @param enabled True to enable AutoAddDevices, False to disable
     */
    void SetAutoAddDevices(bool enabled);

    /**
     * @return The path of the config file.
     */
    const std::string& GetPath();

    /**
     * @param path The new path of the config file.
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
