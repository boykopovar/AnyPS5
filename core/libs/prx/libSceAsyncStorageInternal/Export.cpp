#include "prx/libSceAsyncStorageInternal/include/LegacyExports.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAsyncStorageInternal/include/InventoryExports.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int asyncStorageErrorNull = static_cast<int>(0x81860001);
constexpr int asyncStorageErrorBusy = static_cast<int>(0x8186000d);

std::atomic<void*> g_handlerSlot0{nullptr};
std::atomic<void*> g_handlerSlot1{nullptr};
std::mutex g_counterMutex;
std::uint32_t g_counter = 0;
std::uint64_t g_valueA = 0;
std::uint64_t g_valueB = 0;

}

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI libSceAsyncStorageInternalUnknown__plus_YiZLBbdEy8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+YiZLBbdEy8", libSceAsyncStorageInternalUnknown__plus_YiZLBbdEy8_nid_no_patch);

int APS5_VABI libSceAsyncStorageInternalUnknown_65H7_minus_ohmD9c_nid_no_patch(std::uint32_t* outCount, std::uint32_t reset) {
    if (outCount == nullptr) {
        return asyncStorageErrorNull;
    }
    std::lock_guard<std::mutex> lock(g_counterMutex);
    *outCount = g_counter;
    if (reset != 0u) {
        g_counter = 0u;
    }
    return 0;
}
APS5_EXPORT("65H7-ohmD9c", libSceAsyncStorageInternalUnknown_65H7_minus_ohmD9c_nid_no_patch);

int APS5_VABI libSceAsyncStorageInternalUnknown_93CwGWrhf_plus_w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("93CwGWrhf+w", libSceAsyncStorageInternalUnknown_93CwGWrhf_plus_w_nid_no_patch);

int APS5_VABI libSceAsyncStorageInternalUnknown_9zqbh2tZtZg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9zqbh2tZtZg", libSceAsyncStorageInternalUnknown_9zqbh2tZtZg_nid_no_patch);

int APS5_VABI libSceAsyncStorageInternalUnknown_EKjWxMVMPzY_nid_no_patch(void* handler) {
    if (handler == nullptr) {
        return asyncStorageErrorNull;
    }
    void* expected = nullptr;
    if (!g_handlerSlot0.compare_exchange_strong(expected, handler, std::memory_order_acq_rel)) {
        return asyncStorageErrorBusy;
    }
    return 0;
}
APS5_EXPORT("EKjWxMVMPzY", libSceAsyncStorageInternalUnknown_EKjWxMVMPzY_nid_no_patch);

int APS5_VABI libSceAsyncStorageInternalUnknown_Knldkc0w9lI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Knldkc0w9lI", libSceAsyncStorageInternalUnknown_Knldkc0w9lI_nid_no_patch);

int APS5_VABI libSceAsyncStorageInternalUnknown_TN9xRCVggRw_nid_no_patch() {
    g_handlerSlot0.store(nullptr, std::memory_order_release);
    g_handlerSlot1.store(nullptr, std::memory_order_release);
    return 0;
}
APS5_EXPORT("TN9xRCVggRw", libSceAsyncStorageInternalUnknown_TN9xRCVggRw_nid_no_patch);

int APS5_VABI libSceAsyncStorageInternalUnknown_hP_plus_HenjaOi8_nid_no_patch(std::uint64_t* outValue, std::uint32_t reset) {
    if (outValue == nullptr) {
        return asyncStorageErrorNull;
    }
    std::lock_guard<std::mutex> lock(g_counterMutex);
    *outValue = g_valueA;
    if (reset != 0u) {
        g_valueA = g_valueB;
    }
    return 0;
}
APS5_EXPORT("hP+HenjaOi8", libSceAsyncStorageInternalUnknown_hP_plus_HenjaOi8_nid_no_patch);

int APS5_VABI libSceAsyncStorageInternalUnknown_iYDbqYNVajM_nid_no_patch(std::uint64_t* outValue) {
    if (outValue == nullptr) {
        return asyncStorageErrorNull;
    }
    std::lock_guard<std::mutex> lock(g_counterMutex);
    *outValue = g_valueB;
    return 0;
}
APS5_EXPORT("iYDbqYNVajM", libSceAsyncStorageInternalUnknown_iYDbqYNVajM_nid_no_patch);

int APS5_VABI libSceAsyncStorageInternalUnknown_jR1uFCw8qwc_nid_no_patch(void* handler) {
    if (handler == nullptr) {
        return asyncStorageErrorNull;
    }
    void* expected = nullptr;
    if (!g_handlerSlot1.compare_exchange_strong(expected, handler, std::memory_order_acq_rel)) {
        return asyncStorageErrorBusy;
    }
    return 0;
}
APS5_EXPORT("jR1uFCw8qwc", libSceAsyncStorageInternalUnknown_jR1uFCw8qwc_nid_no_patch);

int APS5_VABI libSceAsyncStorageInternalUnknown_mvZ_minus_UdW_plus_Z_minus_Q_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mvZ-UdW+Z-Q", libSceAsyncStorageInternalUnknown_mvZ_minus_UdW_plus_Z_minus_Q_nid_no_patch);

int APS5_VABI libSceAsyncStorageInternalUnknown_q4KNqhT5ibk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("q4KNqhT5ibk", libSceAsyncStorageInternalUnknown_q4KNqhT5ibk_nid_no_patch);

std::uint64_t APS5_VABI libSceAsyncStorageInternalUnknown_r299OxwCYsY_nid_no_patch() {
    return 0;
}
APS5_EXPORT("r299OxwCYsY", libSceAsyncStorageInternalUnknown_r299OxwCYsY_nid_no_patch);

}

extern "C" {

int APS5_VABI asyncStorageLegacyUnknown_AdTjrblPbkA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AdTjrblPbkA", asyncStorageLegacyUnknown_AdTjrblPbkA_nid_no_patch);

int APS5_VABI asyncStorageLegacyUnknown_AtEWh5hZJAI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AtEWh5hZJAI", asyncStorageLegacyUnknown_AtEWh5hZJAI_nid_no_patch);

int APS5_VABI asyncStorageLegacyUnknown_FlnrIbnUvyI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("FlnrIbnUvyI", asyncStorageLegacyUnknown_FlnrIbnUvyI_nid_no_patch);

int APS5_VABI asyncStorageLegacyUnknown_GETCq_plus_kyXcA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("GETCq+kyXcA", asyncStorageLegacyUnknown_GETCq_plus_kyXcA_nid_no_patch);

int APS5_VABI asyncStorageLegacyUnknown_J_minus_cJO5MPuwo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("J-cJO5MPuwo", asyncStorageLegacyUnknown_J_minus_cJO5MPuwo_nid_no_patch);

int APS5_VABI asyncStorageLegacyUnknown_QM3_minus_5jVrNHg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QM3-5jVrNHg", asyncStorageLegacyUnknown_QM3_minus_5jVrNHg_nid_no_patch);

int APS5_VABI asyncStorageLegacyUnknown_qenXW57P9rU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qenXW57P9rU", asyncStorageLegacyUnknown_qenXW57P9rU_nid_no_patch);

int APS5_VABI asyncStorageLegacyUnknown_smItNfY3yHA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("smItNfY3yHA", asyncStorageLegacyUnknown_smItNfY3yHA_nid_no_patch);

int APS5_VABI asyncStorageLegacyUnknown_yLYSOLZmYso_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yLYSOLZmYso", asyncStorageLegacyUnknown_yLYSOLZmYso_nid_no_patch);

}
