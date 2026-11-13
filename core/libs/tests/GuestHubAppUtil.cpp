#include "prx/libSceHubAppUtil/libSceHubAppUtil.hpp"

#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int NotAvailable = static_cast<int>(0x818D0001u);

}

int main() {
    Require(sceHubAppUtilDeeplinkToLiveDetailView() == NotAvailable);
    Require(sceHubAppUtilDeeplinkToPSSubscriptions() == NotAvailable);
    Require(sceHubAppUtilGetInstallationStatus() == NotAvailable);
    return 0;
}
