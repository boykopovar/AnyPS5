#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceSystemLogger2NativeQueueClient/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_SYSTEM_LOGGER2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x818C0002u);
constexpr int SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED = static_cast<int>(0x818C0004u);
constexpr int SCE_SYSTEM_LOGGER2_GET_EVENT_RESULT = static_cast<int>(0x818C0006u);
constexpr std::size_t eventBytes = 0x72F4;
constexpr std::size_t checkResultBytes = 0x40;

void ZeroGuest(void* destination, std::size_t bytes, const char* function) {
    static const unsigned char zeros[eventBytes] = {};
    if (!GuestCopyOut_nid_no_patch(destination, zeros, bytes)) NotImplemented_nid_no_patch(function);
}

} // namespace

extern "C" {

int APS5_VABI sceSystemLogger2NativeQueueClientCheckAndMaskData() {
    return SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceSystemLogger2NativeQueueClientGetEvent(void* event) {
    if (event == nullptr) return SCE_SYSTEM_LOGGER2_ERROR_INVALID_ARGUMENT;
    ZeroGuest(event, eventBytes, __func__);
    return SCE_SYSTEM_LOGGER2_GET_EVENT_RESULT;
}

int APS5_VABI sceSystemLogger2NativeQueueClientSetDeliveryEnable() {
    return SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceSystemLogger2NativeQueueClientSetDeliveryResult() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSystemLogger2NativeQueueClientSetNativeQueueConfig() {
    return SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceSystemLogger2NativeQueueClientSetBatchDeliveryResult() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSystemLogger2NativeQueueClientSetEventFilter() {
    return SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceSystemLogger2NativeQueueClientCheckData(const void*, void* result) {
    if (result == nullptr) return SCE_SYSTEM_LOGGER2_ERROR_INVALID_ARGUMENT;
    ZeroGuest(result, checkResultBytes, __func__);
    return sceSystemLogger2NativeQueueClientCheckAndMaskData();
}

int APS5_VABI sceSystemLogger2NativeQueueClientGetEvent2() {
    return SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceSystemLogger2NativeQueueClientInitialize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSystemLogger2NativeQueueClientTerminate() {
    return SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI libSceSystemLogger2NativeQueueClientUnknown_AuwwRl6zAcU_nid_no_patch() {
    return SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("AuwwRl6zAcU", libSceSystemLogger2NativeQueueClientUnknown_AuwwRl6zAcU_nid_no_patch);

}
