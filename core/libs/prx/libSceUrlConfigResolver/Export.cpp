#include "prx/libSceUrlConfigResolver/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

constexpr int kInvalidArgument = static_cast<int>(0x80BA8080u);

std::mutex g_stateMutex;
bool g_configLoaded = false;

void ZeroGuest(void* destination, std::uint32_t size, const char* function) {
    static constexpr std::array<std::byte, 4096> zeros{};
    auto* cursor = static_cast<std::byte*>(destination);
    std::uint32_t remaining = size;
    while (remaining != 0) {
        const std::uint32_t chunk = remaining < zeros.size() ? remaining : static_cast<std::uint32_t>(zeros.size());
        if (!GuestCopyOut_nid_no_patch(cursor, zeros.data(), chunk))
            throw std::invalid_argument(std::string(function) + ": unwritable buffer");
        cursor += chunk;
        remaining -= chunk;
    }
}

void StoreGuest(std::uint32_t* destination, std::uint32_t value, const char* function) {
    if (!GuestCopyOut_nid_no_patch(destination, &value, sizeof(value)))
        throw std::invalid_argument(std::string(function) + ": unwritable output");
}

} // namespace

extern "C" {

int APS5_VABI sceUrlConfigResolverGetAppConfig(const char* key, int parameter, void* buffer, std::uint32_t size, std::uint32_t* length) {
    (void)parameter;
    if (buffer != nullptr) ZeroGuest(buffer, size, __func__);
    if (length != nullptr) StoreGuest(length, 0, __func__);
    if (key == nullptr) return kInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceUrlConfigResolverGetDefaultQueryParameter() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUrlConfigResolverGetDeviceId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUrlConfigResolverGetJscHeapSizeSoftLimit(std::uint32_t* limit) {
    if (limit == nullptr) return kInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceUrlConfigResolverGetReleaseVersion() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUrlConfigResolverGetSysVer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceUrlConfigResolverIsUpdated() {
    const std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!g_configLoaded) return false;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceUrlConfigResolverUpdate(const char* parameter) {
    if (parameter == nullptr) return kInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

}
