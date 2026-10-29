#pragma once

#include <string>
#include "prx/libSceSystemService/HostUrlPolicy.hpp"

class HostBrowserLauncher final : public IUrlLauncher {
public:
    HostBrowserLauncher();
    explicit HostBrowserLauncher(std::string program);
    bool Launch(const std::string& url) override;

private:
    std::string program;
};
