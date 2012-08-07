#include "helpers.h"

#include <X11/extensions/XInput2.h>

void StartServer(std::string prefix, ::xorg::testing::XServer &server, XOrgConfig &config) {
    std::stringstream conf;
    conf << "/tmp/" << prefix << ".conf";

    std::stringstream logfile;
    logfile << "/tmp/Xorg-" << prefix << ".log";

    config.SetPath(conf.str());
    config.WriteConfig();
    server.SetOption("-config", config.GetPath());
    server.SetOption("-logfile", logfile.str());
    server.SetDisplayNumber(133);
    server.Start();
    server.WaitForConnections();
}

void KillServer(::xorg::testing::XServer &server, XOrgConfig &config) {
    if (!server.Terminate(3000))
        server.Kill(3000);
}

int FindInputDeviceByName(Display *dpy, const std::string &device_name)
{
    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(dpy, XIAllDevices, &ndevices);

    int found = 0;
    while(ndevices--) {
        if (strcmp(info[ndevices].name, device_name.c_str()) == 0)
            found++;
    }

    XIFreeDeviceInfo(info);

    return found;
}
