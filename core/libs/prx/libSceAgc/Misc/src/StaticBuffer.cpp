#include "prx/libSceAgc/Misc/include/StaticBuffer.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"

namespace {

constexpr int GRAPHICS5_ERROR_INVALID_STATIC_BUFFER = static_cast<int>(0x8a6c000bu);
constexpr std::uint32_t StaticBufferCount = 6;
constexpr std::size_t StaticBufferBytes = 16;
constexpr std::size_t StaticQwordOffset = 16;
constexpr std::uint32_t StaticQwordCount = static_cast<std::uint32_t>((StaticBufferCount * StaticBufferBytes - StaticQwordOffset) / sizeof(std::uint64_t));

std::uint8_t* StaticTable() {
    return static_cast<std::uint8_t*>(AgcDriverStaticTable_nid_postfix());
}

}

extern "C" {

int APS5_VABI sceAgcSetStaticBuffer(std::uint32_t index, const void* buffer) {
    if (index >= StaticBufferCount) return GRAPHICS5_ERROR_INVALID_STATIC_BUFFER;
    std::array<std::uint8_t, StaticBufferBytes> words{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(words.data(), buffer, words.size());
    if (!copied) {
        errno = savedErrno;
        return GRAPHICS5_ERROR_INVALID_STATIC_BUFFER;
    }
    std::memcpy(StaticTable() + std::size_t{index} * StaticBufferBytes, words.data(), words.size());
    errno = savedErrno;
    return 0;
}

int APS5_VABI sceAgcGetStaticBuffer(std::uint32_t index, void* buffer) {
    if (index >= StaticBufferCount) return GRAPHICS5_ERROR_INVALID_STATIC_BUFFER;
    std::array<std::uint8_t, StaticBufferBytes> words{};
    std::memcpy(words.data(), StaticTable() + std::size_t{index} * StaticBufferBytes, words.size());
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(buffer, words.data(), words.size())) {
        errno = savedErrno;
        return GRAPHICS5_ERROR_INVALID_STATIC_BUFFER;
    }
    errno = savedErrno;
    return 0;
}

int APS5_VABI sceAgcSetStaticQword(std::uint32_t index, std::uint64_t value) {
    if (index >= StaticQwordCount) return GRAPHICS5_ERROR_INVALID_STATIC_BUFFER;
    std::memcpy(StaticTable() + StaticQwordOffset + std::size_t{index} * sizeof(value), &value, sizeof(value));
    return 0;
}

int APS5_VABI sceAgcGetStaticQword(std::uint32_t index, std::uint64_t* value) {
    if (index >= StaticQwordCount) return GRAPHICS5_ERROR_INVALID_STATIC_BUFFER;
    std::memcpy(value, StaticTable() + StaticQwordOffset + std::size_t{index} * sizeof(*value), sizeof(*value));
    return 0;
}

}
