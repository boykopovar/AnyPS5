#include "prx/libSceCommonDialog/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"

static std::atomic<bool> g_initialized{false};

extern "C" {

int APS5_VABI sceCommonDialogInitialize(void) {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        return COMMON_DIALOG_ERROR_ALREADY_SYSTEM_INITIALIZED;
    }
    return COMMON_DIALOG_OK;
}

bool APS5_VABI sceCommonDialogIsUsed(void) {
 return false;
}

}
