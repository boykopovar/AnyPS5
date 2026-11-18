#include "prx/libSceSystemLogger2Delivery/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_SYSTEM_LOGGER2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x818C0002u);
constexpr int SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED = static_cast<int>(0x818C0004u);
constexpr int SCE_SYSTEM_LOGGER2_ERROR_NOT_AVAILABLE = static_cast<int>(0x818C000Eu);

std::atomic<bool> g_deliveryInitialized{false};
std::atomic<bool> g_deliverySuspended{false};

}

extern "C" {

int APS5_VABI sceSystemLogger2DeliveryInitialize(void) {
    g_deliveryInitialized.store(true);
    return 0;
}

int APS5_VABI sceSystemLogger2DeliveryTerminate(void) {
    g_deliveryInitialized.store(false);
    return 0;
}

int APS5_VABI sceSystemLogger2DeliverySuspend(void) {
    if (!g_deliveryInitialized.load()) return SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED;
    g_deliverySuspended.store(true);
    return 0;
}

int APS5_VABI sceSystemLogger2DeliveryResume(void) {
    if (!g_deliveryInitialized.load()) return SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED;
    g_deliverySuspended.store(false);
    return 0;
}

int APS5_VABI sceSystemLogger2DeliveryGetBatchResult(void) {
    if (!g_deliveryInitialized.load()) return SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED;
    return SCE_SYSTEM_LOGGER2_ERROR_NOT_AVAILABLE;
}

int APS5_VABI sceSystemLogger2DeliveryGetResult(void) {
    if (!g_deliveryInitialized.load()) return SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED;
    return SCE_SYSTEM_LOGGER2_ERROR_NOT_AVAILABLE;
}

int APS5_VABI sceSystemLogger2DeliverySetBatchDeliveryEvent(void) {
    if (!g_deliveryInitialized.load()) return SCE_SYSTEM_LOGGER2_ERROR_NOT_INITIALIZED;
    return SCE_SYSTEM_LOGGER2_ERROR_NOT_AVAILABLE;
}

}
