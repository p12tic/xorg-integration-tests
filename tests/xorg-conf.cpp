#include <fstream>
#include <sstream>
#include <unistd.h>

#include "xorg-conf.h"

XOrgConfig::XOrgConfig(const std::string& path) {
    config_file = path;

    std::stringstream section;
    section << ""
        "Section \"ServerFlags\"\n"
        "     Option \"Log\" \"flush\"\n"
        "EndSection\n";

    sections.push_back(section.str());
    auto_add_devices = false;
}

void XOrgConfig::WriteConfig(const std::string &param) {
    if (!param.empty()) {
        config_file = param;
        SetPath(param);
    }

    std::ofstream conffile(config_file.c_str());
    conffile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    conffile << ""
        "Section \"ServerLayout\"\n"
        "    Identifier \"Dummy layout\"\n";
    if (!auto_add_devices)
        conffile << "    Option \"AutoAddDevices\" \"off\"\n";
    if (!default_device.empty())
        conffile << "    Screen 0 \"" << default_device << " screen\" 0 0\n";
    std::vector<std::string>::const_iterator it;
    for (it = input_devices.begin(); it != input_devices.end(); it++)
        conffile << "    InputDevice \"" << *it << "\"\n";
    conffile << "EndSection\n";

    for (it = sections.begin(); it != sections.end(); it++)
        conffile << "\n" << *it;
}

void XOrgConfig::RemoveConfig() {
    unlink(GetPath().c_str());
}

void XOrgConfig::AddDefaultScreenWithDriver(const std::string &driver,
                                            const std::string &identifier,
                                            const std::string &options) {
    default_device = identifier;

    std::stringstream section;
    section << "Section \"Device\"\n"
               "    Identifier \"" << identifier << "\"\n"
               "    Driver \"" << driver << "\"\n"
               << options << "\n" <<
               "EndSection\n";
    sections.push_back(section.str());

    section.str(std::string());
    section << "Section \"Screen\"\n"
               "    Identifier \"" << identifier << " screen\"\n"
               "    Device \"" << identifier << "\"\n"
               "EndSection\n";
    sections.push_back(section.str());
}

void XOrgConfig::AddInputSection(const std::string &driver,
                                 const std::string &identifier,
                                 const std::string &options,
                                 bool reference_from_layout) {
    if (reference_from_layout)
        input_devices.push_back(identifier);

    std::stringstream section;
    section << "Section \"InputDevice\"\n"
               "    Identifier \"" << identifier << "\"\n"
               "    Driver \"" << driver << "\"\n"
               << options << "\n" <<
               "EndSection\n";
    sections.push_back(section.str());
}

void XOrgConfig::SetAutoAddDevices(bool enabled) {
    auto_add_devices = enabled;
}

const std::string& XOrgConfig::GetPath() {
    return config_file;
}
void XOrgConfig::SetPath(const std::string& path) {
    config_file = path;
}
