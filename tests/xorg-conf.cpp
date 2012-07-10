#include <fstream>
#include <sstream>

#include "xorg-conf.h"

XOrgConfig::XOrgConfig(const std::string path) {
    config_file = path;

    std::stringstream section;
    section << ""
        "Section \"ServerFlags\"\n"
        "     Option \"Log\" \"flush\"\n"
        "EndSection\n";

    sections.push_back(section.str());

    section.str(std::string());
    section << ""
        "Section \"Screen\"\n"
        "    Identifier \"Dummy screen\"\n"
        "    Device \"Dummy video device\"\n"
        "EndSection\n";
    sections.push_back(section.str());

    section.str(std::string());
    section << ""
        "Section \"Device\"\n"
        "    Identifier \"Dummy video device\"\n"
        "    Driver \"dummy\"\n"
        "EndSection\n";
    sections.push_back(section.str());
}

void XOrgConfig::WriteConfig(const std::string &param) {
    if (!param.empty())
        config_file = param;

    std::ofstream conffile(config_file.c_str());
    conffile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    conffile << ""
        "Section \"ServerLayout\"\n"
        "    Identifier \"Dummy layout\"\n"
        "    Screen 0 \"Dummy screen\" 0 0\n"
        "    Option \"AutoAddDevices\" \"off\"\n";
    std::vector<std::string>::const_iterator it;
    for (it = input_devices.begin(); it != input_devices.end(); it++)
        conffile << "    InputDevice \"" << *it << "\"\n";

    conffile << "EndSection\n";

    for (it = sections.begin(); it != sections.end(); it++)
        conffile << "\n" << *it;
}

void XOrgConfig::AddInputSection(std::string driver,
                                      std::string identifier,
                                      std::string options,
                                      bool reference_from_layout) {
    if (reference_from_layout)
        input_devices.push_back(identifier);

    std::stringstream section;
    section << "Section \"InputDevice\"\n"
               "    Identifier \"" << identifier << "\"\n"
               "    Driver \"" << driver << "\"\n"
               << options <<
               "EndSection\n";
    sections.push_back(section.str());
}

const std::string& XOrgConfig::GetPath() {
    return config_file;
}
void XOrgConfig::SetPath(const std::string& path) {
    config_file = path;
}
