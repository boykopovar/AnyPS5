#include "prx/libSceNpManager/include/Api.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace {

constexpr int PUSH_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x8055A001);
constexpr int PUSH_ERROR_NOT_INITIALIZED = static_cast<int>(0x8055A002);
constexpr int PUSH_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8055A003);
constexpr int PUSH2_ERROR_NOT_INITIALIZED = static_cast<int>(0x8055C002);
constexpr int PUSH2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8055C003);
constexpr int PUSH2_ERROR_NO_SLOT = static_cast<int>(0x8055C005);
constexpr std::size_t PUSH2_CONTEXT_COUNT = 32;
constexpr std::size_t PUSH2_INIT_PARAMETER_SIZE = 0x18;

std::mutex pushMutex;
bool pushInitialized = false;
std::array<bool, PUSH2_CONTEXT_COUNT> push2Contexts {};
void* push2GlobalMutex = nullptr;

bool AnyPush2Context() {
    return std::any_of(push2Contexts.begin(), push2Contexts.end(), [](bool used) { return used; });
}

}

extern "C" {

int APS5_VABI sceNpPushInit(void) {
    std::lock_guard lock(pushMutex);
    if (pushInitialized) return PUSH_ERROR_ALREADY_INITIALIZED;
    pushInitialized = true;
    return 0;
}

int APS5_VABI sceNpPushTerm(void) {
    std::lock_guard lock(pushMutex);
    if (!pushInitialized) return PUSH_ERROR_NOT_INITIALIZED;
    pushInitialized = false;
    return 0;
}

int APS5_VABI sceNpPushCheckCallback(void) {
    std::lock_guard lock(pushMutex);
    if (!pushInitialized) return PUSH_ERROR_NOT_INITIALIZED;
    return 0;
}

int APS5_VABI sceNpPush2Init(const void* parameter) {
    if (parameter == nullptr) return PUSH2_ERROR_INVALID_ARGUMENT;
    std::uint8_t bytes[PUSH2_INIT_PARAMETER_SIZE] {};
    if (!GuestCopyIn_nid_no_patch(bytes, parameter, sizeof(bytes))) return PUSH2_ERROR_INVALID_ARGUMENT;
    std::uint32_t allocatorKind = 0;
    std::uint32_t mode = 0;
    std::memcpy(&allocatorKind, bytes + 0x10, sizeof(allocatorKind));
    std::memcpy(&mode, bytes + 0x14, sizeof(mode));
    if (allocatorKind > 1 || mode < 1 || mode > 5) return PUSH2_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(pushMutex);
    const auto slot = std::find(push2Contexts.begin(), push2Contexts.end(), false);
    if (slot == push2Contexts.end()) return PUSH2_ERROR_NO_SLOT;
    *slot = true;
    return static_cast<int>(std::distance(push2Contexts.begin(), slot)) + 1;
}

int APS5_VABI sceNpPush2Term(std::int32_t contextId) {
    std::lock_guard lock(pushMutex);
    if (contextId >= 1 && contextId <= static_cast<std::int32_t>(PUSH2_CONTEXT_COUNT)) {
        push2Contexts[static_cast<std::size_t>(contextId) - 1] = false;
    }
    return 0;
}

bool APS5_VABI sceNpPush2IsInit(void) {
    std::lock_guard lock(pushMutex);
    return AnyPush2Context();
}

int APS5_VABI sceNpPush2SetGlobalMutex(void* mutex) {
    if (mutex == nullptr) return PUSH2_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(pushMutex);
    push2GlobalMutex = mutex;
    return 0;
}

int APS5_VABI sceNpPush2OptionalCheckCallback(void) {
    std::lock_guard lock(pushMutex);
    if (!AnyPush2Context()) return PUSH2_ERROR_NOT_INITIALIZED;
    return 0;
}

int APS5_VABI sceNpPush2TriggerEmptyUserEvent(void) {
    std::lock_guard lock(pushMutex);
    if (!AnyPush2Context()) return PUSH_ERROR_NOT_INITIALIZED;
    return 0;
}

int APS5_VABI sceNpPushIntGetConnectionState(void* state) {
    if (state == nullptr) return PUSH_ERROR_INVALID_ARGUMENT;
    std::lock_guard lock(pushMutex);
    if (!pushInitialized) return PUSH_ERROR_NOT_INITIALIZED;
    *static_cast<std::int32_t*>(state) = 0;
    return 0;
}

}
