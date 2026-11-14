#include "prx/libSceSrcUtl/include/Api.hpp"
#include <algorithm>
#include <cerrno>
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
constexpr int SRC_UTILITY_ERROR_FLUSHED = static_cast<int>(0x815D000A);

constexpr int SRC_UTILITY_HANDLE_SIZE = 0x9678;
constexpr std::size_t SRC_UTILITY_PRIORITY_OFFSET = 0x2C;
constexpr std::size_t SRC_UTILITY_STATE_OFFSET = 0x30;
constexpr std::size_t SRC_UTILITY_MAGIC_OFFSET = 0x9470;
constexpr std::size_t SRC_UTILITY_AJM_CONTEXT_OFFSET = 0x9478;
constexpr std::uint32_t SRC_UTILITY_STATE_FLUSHED = 2;
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
    if (handle == nullptr) return false;
    std::uint64_t magic = 0;
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&magic, static_cast<const std::byte*>(handle) + SRC_UTILITY_MAGIC_OFFSET, sizeof(magic))) {
        errno = savedErrno;
        return false;
    }
    errno = savedErrno;
    return magic == SRC_UTILITY_MAGIC;
}

int CheckParameters(std::uint32_t channels, std::uint32_t mode, std::uint32_t inputRate, std::uint32_t outputRate) {
    if (channels == 0 || channels > SRC_UTILITY_MAX_CHANNELS) return SRC_UTILITY_ERROR_INVALID_CHANNELS;
    if (mode > SRC_UTILITY_MAX_MODE) return SRC_UTILITY_ERROR_INVALID_MODE;
    if (!IsRate(SRC_UTILITY_INPUT_RATES, inputRate)) return SRC_UTILITY_ERROR_INVALID_INPUT_RATE;
    if (!IsRate(SRC_UTILITY_OUTPUT_RATES, outputRate)) return SRC_UTILITY_ERROR_INVALID_OUTPUT_RATE;
    return 0;
}

bool ReadHandleWord(const void* handle, std::size_t offset, std::uint32_t& value) {
    const int savedErrno = errno;
    const bool read = GuestCopyIn_nid_no_patch(&value, static_cast<const std::byte*>(handle) + offset, sizeof(value));
    errno = savedErrno;
    return read;
}

}

extern "C" {

int APS5_VABI sceSrcUtilityCreateEx(const std::uint32_t* context, std::uint32_t channels, std::uint32_t mode, std::uint32_t inputRate, std::uint32_t outputRate, void* handle) {
    if (context == nullptr) return SRC_UTILITY_ERROR_INVALID_POINTER;
    if (handle == nullptr) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    const int status = CheckParameters(channels, mode, inputRate, outputRate);
    if (status != 0) return status;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSrcUtilityDeleteEx(void* handle) {
    if (!HandleIsInitialized(handle)) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceSrcUtilityFinalize(void* handle) {
    if (!HandleIsInitialized(handle)) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSrcUtilityFlush(void* handle, std::int32_t argument1, void* argument2, void* argument3) {
    if (!HandleIsInitialized(handle)) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    if (argument1 < 0 || argument2 == nullptr || argument3 == nullptr) return SRC_UTILITY_ERROR_INVALID_POINTER;
    std::uint32_t state = 0;
    if (!ReadHandleWord(handle, SRC_UTILITY_STATE_OFFSET, state)) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    if (state == SRC_UTILITY_STATE_FLUSHED) return SRC_UTILITY_ERROR_FLUSHED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSrcUtilityGetAjmContext(const void* handle, std::uint32_t* context) {
    if (handle == nullptr) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    if (context == nullptr) return 0;
    std::uint32_t value = 0;
    if (!ReadHandleWord(handle, SRC_UTILITY_AJM_CONTEXT_OFFSET, value)) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(context, &value, sizeof(value))) {
        errno = savedErrno;
        return SRC_UTILITY_ERROR_INVALID_POINTER;
    }
    return 0;
}

int APS5_VABI sceSrcUtilityGetDelaySamples() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSrcUtilityGetHandleSize(std::uint32_t channels, std::uint32_t mode, std::uint32_t input_rate, std::uint32_t output_rate) {
    const int status = CheckParameters(channels, mode, input_rate, output_rate);
    return status != 0 ? status : SRC_UTILITY_HANDLE_SIZE;
}

int APS5_VABI sceSrcUtilityGetPriority(const void* handle, std::uint32_t* priority) {
    if (handle == nullptr || !HandleIsInitialized(handle)) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    if (priority == nullptr) return SRC_UTILITY_ERROR_INVALID_POINTER;
    std::uint32_t value = 0;
    std::memcpy(&value, static_cast<const std::byte*>(handle) + SRC_UTILITY_PRIORITY_OFFSET, sizeof(value));
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(priority, &value, sizeof(value))) {
        errno = savedErrno;
        return SRC_UTILITY_ERROR_INVALID_POINTER;
    }
    return 0;
}

int APS5_VABI sceSrcUtilityInitialize(std::uint32_t channels, std::uint32_t mode, std::uint32_t inputRate, std::uint32_t outputRate, void* handle) {
    if (handle == nullptr) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    const int status = CheckParameters(channels, mode, inputRate, outputRate);
    if (status != 0) return status;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSrcUtilityInitializeEx(std::uint32_t* context) {
    if (context == nullptr) return SRC_UTILITY_ERROR_INVALID_POINTER;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSrcUtilityResample(void* handle, std::int32_t argument1) {
    if (!HandleIsInitialized(handle)) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    if (argument1 < 0) return SRC_UTILITY_ERROR_INVALID_POINTER;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSrcUtilitySetPriority(void* handle, std::uint32_t priority) {
    if (handle == nullptr || !HandleIsInitialized(handle)) return SRC_UTILITY_ERROR_INVALID_HANDLE;
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(static_cast<std::byte*>(handle) + SRC_UTILITY_PRIORITY_OFFSET, &priority, sizeof(priority))) {
        errno = savedErrno;
        return SRC_UTILITY_ERROR_INVALID_HANDLE;
    }
    errno = savedErrno;
    return 0;
}
int APS5_VABI sceSrcUtilityTerminateEx() {
    NotImplemented_nid_no_patch(__func__);
}


}
