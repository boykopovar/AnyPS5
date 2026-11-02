#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceVideoNativeExtEssential/include/InventoryExports.hpp"
#include "prx/libSceVideoNativeExtEssential/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
std::atomic<bool> g_initialized{false};
}

extern "C" {

int APS5_VABI sceVideoNativeExtEssential_CreateDecInstance() {
    if (!g_initialized.load()) {
        NotImplemented_nid_no_patch(__func__);
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoNativeExtEssential_DestroyDecInstance() {
    if (!g_initialized.load()) {
        NotImplemented_nid_no_patch(__func__);
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoNativeExtEssential_Finalize() {
    bool expected = true;
    g_initialized.compare_exchange_strong(expected, false);
    return 0;
}

int APS5_VABI sceVideoNativeExtEssential_Initialize() {
    bool expected = false;
    g_initialized.compare_exchange_strong(expected, true);
    return 0;
}

int APS5_VABI sceVideoNativeExtEssential_IsInit() {
    return g_initialized.load() ? 1 : 0;
}

}

extern "C" {

int APS5_VABI libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("iwAQhvb9BvY", libSceVideoNativeExtEssentialUnknown_iwAQhvb9BvY_nid_no_patch);

}
