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

class HostMessageBoxConfirmation final : public IUrlConfirmation {
public:
    bool Confirm(const std::string& url) override;
};

std::string HostConfirmationText(const std::string& url);

extern "C" void SystemServiceSetUrlHandlers_nid_no_patch(IUrlLauncher* launcher, IUrlConfirmation* confirmation);
