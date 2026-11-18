#include "prx/libSceHubAppUtil/libSceHubAppUtil.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    Require(sceHubAppUtilDeeplinkToLiveDetailView() == 0);
    Require(sceHubAppUtilDeeplinkToPSSubscriptions() == 0);
    Require(sceHubAppUtilGetInstallationStatus() == 0);
    return 0;
}
