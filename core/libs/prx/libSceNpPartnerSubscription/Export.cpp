#include "prx/libSceNpPartnerSubscription/include/InventoryExports.hpp"
#include <atomic>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::atomic<bool> g_initialized{false};

}

extern "C" {

int APS5_VABI sceNpPartnerSubscriptionInitialize(void) {
    g_initialized.store(true);
    return 0;
}

int APS5_VABI sceNpPartnerSubscriptionTerminate(void) {
    g_initialized.store(false);
    return 0;
}

}
