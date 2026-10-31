#include "prx/libSceNpPartnerSubscription/include/InventoryExports.hpp"
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    Require(sceNpPartnerSubscriptionInitialize() == 0);
    Require(sceNpPartnerSubscriptionTerminate() == 0);
    Require(sceNpPartnerSubscriptionInitialize() == 0);
    Require(sceNpPartnerSubscriptionTerminate() == 0);
}
