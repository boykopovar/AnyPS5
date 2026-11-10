#include "prx/libSceCommonDialog/include/Api.hpp"
#include "prx/libSceCommonDialog/include/DialogUse.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"

static std::atomic<bool> g_initialized{false};
static std::atomic<bool> g_used{false};

extern "C" {

int APS5_VABI sceCommonDialogInitialize(void) {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        return COMMON_DIALOG_ERROR_ALREADY_SYSTEM_INITIALIZED;
    }
    return COMMON_DIALOG_OK;
}

bool APS5_VABI sceCommonDialogIsUsed(void) {
    return g_used.load();
}

bool CommonDialogSystemInitialized_nid_no_patch() {
    return g_initialized.load();
}

bool CommonDialogAcquire_nid_no_patch() {
    bool expected = false;
    return g_used.compare_exchange_strong(expected, true);
}

void CommonDialogRelease_nid_no_patch() {
    g_used.store(false);
}

}
