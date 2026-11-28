#include "prx/libSceFlexContent/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorNotInitialized = static_cast<int>(0x81B60001);
constexpr int ErrorAlreadyInitialized = static_cast<int>(0x81B60002);

std::atomic<bool> g_initialized{false};

}

extern "C" {

int APS5_VABI sceFlexContentInitialize(void) {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        return ErrorAlreadyInitialized;
    }
    return 0;
}

int APS5_VABI sceFlexContentTerminate(void) {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) {
        return ErrorNotInitialized;
    }
    return 0;
}

APS5_EXPORT("7n4PI+iTKws", sceFlexContentUnknown00);
int APS5_VABI sceFlexContentUnknown00(void) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch("7n4PI+iTKws");
    return 0;
}

APS5_EXPORT("bgXJD9PYROo", sceFlexContentUnknown01);
int APS5_VABI sceFlexContentUnknown01(void) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch("bgXJD9PYROo");
    return 0;
}

int APS5_VABI sceFlexContentMount(void) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("EP5ByjgbB2Y", sceFlexContentUnknown02);
int APS5_VABI sceFlexContentUnknown02(void) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch("EP5ByjgbB2Y");
    return 0;
}

int APS5_VABI sceFlexContentGetDownloadProgress(void) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFlexContentDelete(void) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFlexContentGetLatestVersion(void) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFlexContentGetInstalledVersion(void) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("TNwR02XaltA", sceFlexContentUnknown03);
int APS5_VABI sceFlexContentUnknown03(void) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch("TNwR02XaltA");
    return 0;
}

APS5_EXPORT("EQWWghng4OI", sceFlexContentUnknown04);
int APS5_VABI sceFlexContentUnknown04(void) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch("EQWWghng4OI");
    return 0;
}

}
