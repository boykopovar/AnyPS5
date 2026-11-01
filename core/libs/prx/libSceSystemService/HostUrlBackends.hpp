#pragma once

#include <atomic>
#include <memory>
#include <string>
#include "prx/libSceSystemService/HostUrlPolicy.hpp"

constexpr int HOST_BROWSER_MAX_OPENERS = 2;

class HostBrowserLauncher final : public IUrlLauncher {
public:
    HostBrowserLauncher();
    explicit HostBrowserLauncher(std::string program);
    bool Launch(const std::string& url) override;

private:
    std::string program;
    std::shared_ptr<std::atomic<int>> running = std::make_shared<std::atomic<int>>(0);
};

class HostMessageBoxConfirmation final : public IUrlConfirmation {
public:
    bool Confirm(const std::string& url) override;
};

std::string HostConfirmationText(const std::string& url);

extern "C" void SystemServiceSetUrlHandlers_nid_no_patch(IUrlLauncher* launcher, IUrlConfirmation* confirmation, IUrlClock* clock);
