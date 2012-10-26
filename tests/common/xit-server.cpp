#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "xit-server.h"
#include "helpers.h"

using namespace xorg::testing;

XITServer::XITServer() : XServer::XServer() {
    SetOption("-logfile", GetDefaultLogFile());
    SetOption("-config", GetDefaultConfigFile());
    SetDisplayNumber(133);
}
