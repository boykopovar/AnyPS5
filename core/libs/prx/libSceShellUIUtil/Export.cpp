#include "prx/libSceShellUIUtil/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorNotInitialized = static_cast<int>(0x80BA0001u);
constexpr int ErrorInvalidArgument = static_cast<int>(0x80BA0020u);
constexpr std::size_t StringLimit = 0x1000;
constexpr std::uint32_t AppUrlCapacityLimit = 0x1000;
constexpr std::uint32_t TaskOutputMinimumCapacity = 0x1000;
constexpr std::uint32_t LaunchParameterSize = 8;

std::recursive_mutex g_mutex;
bool g_initialized = false;
std::atomic<std::uint32_t> g_taskTokens{0};

bool IsValidGuestWrite(void* pointer, std::size_t size) {
    if (pointer == nullptr) return false;
    const bool writable = GuestWritable_nid_no_patch(pointer, size != 0 ? size : 1);
    return writable;
}

bool StringTooLong(const char* text) {
    if (text == nullptr) return true;
    std::string copy;
    const int error = CopyGuestString_nid_no_patch(text, StringLimit, copy);
    return error != 0 || copy.length() > StringLimit - 1;
}

int CallService(const char* name) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) {
        return ErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(name);
}

}

extern "C" {

int APS5_VABI sceShellUIUtilAbortTask(std::int32_t taskToken) {
    (void)taskToken;
    return CallService(__func__);
}

int APS5_VABI sceShellUIUtilCreateTaskToken() {
    return static_cast<int>(g_taskTokens.fetch_add(1) + 1);
}

int APS5_VABI sceShellUIUtilGetAppUrl(const char* application, const char* parameter, char* output, std::uint32_t capacity, std::uint32_t* outputSize) {
    if (application == nullptr || parameter == nullptr || StringTooLong(application)) {
        return ErrorInvalidArgument;
    }
    if (capacity > AppUrlCapacityLimit || StringTooLong(parameter) || (capacity != 0 && !IsValidGuestWrite(output, capacity))) {
        return ErrorInvalidArgument;
    }
    if (outputSize != nullptr && !IsValidGuestWrite(outputSize, sizeof(*outputSize))) {
        return ErrorInvalidArgument;
    }
    return CallService(__func__);
}

int APS5_VABI sceShellUIUtilInitialize() {
    std::lock_guard lock(g_mutex);
    g_initialized = true;
    return 0;
}

int APS5_VABI sceShellUIUtilLaunchByUri(const char* uri, const ShellUIUtilLaunchParameter* parameter) {
    if (uri == nullptr || parameter == nullptr || StringTooLong(uri)) {
        return ErrorInvalidArgument;
    }
    std::uint32_t size = 0;
    const bool copied = GuestCopyIn_nid_no_patch(&size, parameter, sizeof(size));
    if (!copied || size != LaunchParameterSize) {
        return ErrorInvalidArgument;
    }
    return CallService(__func__);
}

int APS5_VABI sceShellUIUtilLaunchTask(std::int32_t taskToken, const char* uri, char* output, std::uint32_t capacity) {
    (void)taskToken;
    if (uri == nullptr || capacity < TaskOutputMinimumCapacity || output == nullptr || StringTooLong(uri)) {
        return ErrorInvalidArgument;
    }
    const std::vector<char> zeros(capacity, 0);
    const bool copied = GuestCopyOut_nid_no_patch(output, zeros.data(), capacity);
    if (!copied) {
        return ErrorInvalidArgument;
    }
    return CallService(__func__);
}

int APS5_VABI sceShellUIUtilTerminate() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) {
        return ErrorNotInitialized;
    }
    g_initialized = false;
    return 0;
}

}
