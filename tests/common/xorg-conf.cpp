/*
 * Copyright © 2012-2013 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <fstream>
#include <sstream>
#include <unistd.h>

#include "xorg-conf.h"
#include "helpers.h"
#include "xit-server.h"

XOrgConfig::XOrgConfig(const std::string& path) {
    if (path.empty())
        SetPath(XITServer::GetDefaultConfigFile());

    std::stringstream section;
    section << ""
        "Section \"ServerFlags\"\n"
        "     Option \"Log\" \"flush\"\n"
        "EndSection\n";

    sections.push_back(section.str());
    auto_add_devices = false;
}

void XOrgConfig::WriteConfig(const std::string &path) {
    if (!path.empty()) {
        config_file = path;
        SetPath(path);
    }

    std::ofstream conffile(config_file.c_str());
    conffile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    bool need_server_layout = !auto_add_devices || !default_device.empty() || input_devices.size() > 0;

    std::vector<std::string>::const_iterator it;
    if (need_server_layout) {
        conffile << ""
            "Section \"ServerLayout\"\n"
            "    Identifier \"Dummy layout\"\n";
        if (!auto_add_devices)
            conffile << "    Option \"AutoAddDevices\" \"off\"\n";
        if (!default_device.empty())
            conffile << "    Screen 0 \"" << default_device << " screen\" 0 0\n";
        for (it = input_devices.begin(); it != input_devices.end(); it++)
            conffile << "    InputDevice \"" << *it << "\"\n";
        for (it = server_layout_opts.begin(); it != server_layout_opts.end(); ++it)
            conffile << *it << "\n";
        conffile << "EndSection\n";
    }

    for (it = sections.begin(); it != sections.end(); it++)
        conffile << "\n" << *it;
}

void XOrgConfig::AppendRawConfig(const std::string &config) {
    sections.push_back(config);
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

void XOrgConfig::AddServerLayoutOption(const std::string& option)
{
    server_layout_opts.push_back(option);
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

void XOrgConfig::AddInputClass(const std::string &identifier,
                               const std::string &matches,
                               const std::string &options) {
    std::stringstream section;
    section << "Section \"InputClass\"\n"
            << "    Identifier \"" << identifier << "\"\n"
            << matches << "\n"
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
