#include "prx/libSceSrcUtl/include/Api.hpp"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iterator>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SRC_UTILITY_ERROR_INVALID_HANDLE = static_cast<int>(0x815D0000);
constexpr int SRC_UTILITY_ERROR_INVALID_POINTER = static_cast<int>(0x815D0001);
constexpr int SRC_UTILITY_ERROR_INVALID_CHANNELS = static_cast<int>(0x815D0002);
constexpr int SRC_UTILITY_ERROR_INVALID_MODE = static_cast<int>(0x815D0003);
constexpr int SRC_UTILITY_ERROR_INVALID_INPUT_RATE = static_cast<int>(0x815D0004);
constexpr int SRC_UTILITY_ERROR_INVALID_OUTPUT_RATE = static_cast<int>(0x815D0005);

constexpr int SRC_UTILITY_HANDLE_SIZE = 0x9678;
constexpr std::size_t SRC_UTILITY_PRIORITY_OFFSET = 0x2C;
constexpr std::size_t SRC_UTILITY_MAGIC_OFFSET = 0x9470;
constexpr std::uint64_t SRC_UTILITY_MAGIC = 0xDEADBEEFBAADF00Dull;
constexpr std::uint32_t SRC_UTILITY_MAX_CHANNELS = 8;
constexpr std::uint32_t SRC_UTILITY_MAX_MODE = 2;
constexpr std::uint32_t SRC_UTILITY_INPUT_RATES[] = {22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000, 128000, 176400, 192000};
constexpr std::uint32_t SRC_UTILITY_OUTPUT_RATES[] = {44100, 48000, 96000, 192000};

template <std::size_t N>
bool IsRate(const std::uint32_t (&rates)[N], std::uint32_t rate) {
    return std::find(std::begin(rates), std::end(rates), rate) != std::end(rates);
}

bool HandleIsInitialized(const void* handle) {
    std::uint64_t magic = 0;
    std::memcpy(&magic, static_cast<const std::byte*>(handle) + SRC_UTILITY_MAGIC_OFFSET, sizeof(magic));
    return magic == SRC_UTILITY_MAGIC;
}

}

extern "C" {

int APS5_VABI sceSrcUtilityCreateEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSrcUtilityDeleteEx() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSrcUtilityFinalize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSrcUtilityFlush() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSrcUtilityGetAjmContext() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSrcUtilityGetDelaySamples() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSrcUtilityGetHandleSize(std::uint32_t channels, std::uint32_t mode, std::uint32_t input_rate, std::uint32_t output_rate) {
    if (channels == 0 || channels > SRC_UTILITY_MAX_CHANNELS) return SRC_UTILITY_ERROR_INVALID_CHANNELS;
    if (mode > SRC_UTILITY_MAX_MODE) return SRC_UTILITY_ERROR_INVALID_MODE;
    if (!IsRate(SRC_UTILITY_INPUT_RATES, input_rate)) return SRC_UTILITY_ERROR_INVALID_INPUT_RATE;
    if (!IsRate(SRC_UTILITY_OUTPUT_RATES, output_rate)) return SRC_UTILITY_ERROR_INVALID_OUTPUT_RATE;
    return SRC_UTILITY_HANDLE_SIZE;
}

int APS5_VABI sceSrcUtilityGetPriority(const void* handle, std::uint32_t* priority) {
    if (handle == nullptr || !HandleIsInitialized(handle)) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    if (priority == nullptr) return SRC_UTILITY_ERROR_INVALID_POINTER;
    std::memcpy(priority, static_cast<const std::byte*>(handle) + SRC_UTILITY_PRIORITY_OFFSET, sizeof(*priority));
    return 0;
}

int APS5_VABI sceSrcUtilityInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSrcUtilityInitializeEx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSrcUtilityResample() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSrcUtilitySetPriority(void* handle, std::uint32_t priority) {
    if (handle == nullptr || !HandleIsInitialized(handle)) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    std::memcpy(static_cast<std::byte*>(handle) + SRC_UTILITY_PRIORITY_OFFSET, &priority, sizeof(priority));
    return 0;
}
int APS5_VABI sceSrcUtilityTerminateEx() {
    NotImplemented_nid_no_patch(__func__);
}


}
