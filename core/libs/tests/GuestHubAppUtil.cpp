#include "prx/libSceHubAppUtil/libSceHubAppUtil.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int NotAvailable = static_cast<int>(0x818D0001u);

}

int main(int argc, char** argv) {
    const bool live = argc < 2 || std::strcmp(argv[1], "live") == 0;
    const bool subscriptions = argc < 2 || std::strcmp(argv[1], "subscriptions") == 0;
    const bool status = argc < 2 || std::strcmp(argv[1], "status") == 0;
    Require(live || subscriptions || status);
    if (live) Require(sceHubAppUtilDeeplinkToLiveDetailView() == NotAvailable);
    if (subscriptions) Require(sceHubAppUtilDeeplinkToPSSubscriptions() == NotAvailable);
    if (status) Require(sceHubAppUtilGetInstallationStatus() == NotAvailable);
    return 0;
}