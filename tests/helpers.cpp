#include "helpers.h"

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
    if (!server.Terminate())
        server.Kill();
    unlink(config.GetPath().c_str());
}
