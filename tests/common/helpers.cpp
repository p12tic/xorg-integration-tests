#if HAVE_CONFIG_H
#include <config.h>
#endif
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

int FindInputDeviceByName(Display *dpy, const std::string &device_name, int *deviceid)
{
    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(dpy, XIAllDevices, &ndevices);

    int found = 0;
    while(ndevices--) {
        if (strcmp(info[ndevices].name, device_name.c_str()) == 0) {
            if (deviceid)
                *deviceid = info[ndevices].deviceid;
            found++;
        }
    }

    if (found > 1) {
        SCOPED_TRACE("More than one device named '" + device_name +
                     "' found.\nThis may cause some tests to fail.\n");
    }

    XIFreeDeviceInfo(info);

    return found;
}
