#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "device-interface.h"

void DeviceInterface::SetDevice(const std::string& path, const std::string &basedir)
{
    dev = std::auto_ptr<xorg::testing::evemu::Device>(
                new xorg::testing::evemu::Device(basedir + path));
}
