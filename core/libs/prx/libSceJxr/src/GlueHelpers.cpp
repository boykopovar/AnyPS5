#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

extern "C" const void* JxrHashLookup_nid_no_patch(std::uint8_t hash);

namespace {

constexpr std::int64_t WmpErrSuccess = 0;
constexpr std::int64_t WmpErrOutOfMemory = -101;
constexpr std::int64_t WmpErrUnsupportedFormat = -106;
constexpr std::uint32_t ImageUnsupported = 100;

struct IidInfo {
    const char* extension;
    const std::uint32_t* encode;
    const std::uint32_t* decode;
};

int Lower(char value) {
    const auto byte = static_cast<unsigned char>(value);
    return byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte;
}

bool MatchesPrefix(const char* extension, const char* expected) {
    auto count = std::strlen(expected);
    for (; Lower(*extension) == Lower(*expected) && *extension && *expected && count; ++extension, ++expected, --count) {}
    return count == 0;
}

const IidInfo* FindIid(const char* extension, const char* function) {
    if (!extension) throw std::invalid_argument(std::string(function) + " requires an extension");
    static const IidInfo table[] = {
        {".jxr", &JxrGuest_IID_PKImageWmpEncode_nid_no_patch, &JxrGuest_IID_PKImageWmpDecode_nid_no_patch},
        {".wdp", &ImageUnsupported, &JxrGuest_IID_PKImageWmpDecode_nid_no_patch},
        {".hdp", &ImageUnsupported, &JxrGuest_IID_PKImageWmpDecode_nid_no_patch},
    };
    for (const auto& entry : table)
        if (MatchesPrefix(extension, entry.extension)) return &entry;
    return nullptr;
}

std::int64_t StoreIid(const std::uint32_t* IidInfo::*field, const char* extension, const std::uint32_t** iid,
                      const char* function) {
    const auto* info = FindIid(extension, function);
    if (!info) return WmpErrUnsupportedFormat;
    if (!iid) throw std::invalid_argument(std::string(function) + " requires an output for a known extension");
    *iid = info->*field;
    return WmpErrSuccess;
}

} // namespace

extern "C" {

const void* APS5_VABI GetPixelFormatFromHash_nid_postfix(std::uint8_t hash) {
    return JxrHashLookup_nid_no_patch(hash);
}

std::int64_t APS5_VABI GetImageEncodeIID_nid_postfix(const char* extension, const std::uint32_t** iid) {
    return StoreIid(&IidInfo::encode, extension, iid, "GetImageEncodeIID");
}

std::int64_t APS5_VABI GetImageDecodeIID_nid_postfix(const char* extension, const std::uint32_t** iid) {
    return StoreIid(&IidInfo::decode, extension, iid, "GetImageDecodeIID");
}

std::int64_t APS5_VABI PKAllocAligned_nid_postfix(void** memory, std::size_t size, std::size_t alignment) {
    if (!memory) throw std::invalid_argument("PKAllocAligned requires an output pointer");
    *memory = nullptr;
    if (alignment == 0) throw std::invalid_argument("PKAllocAligned: zero alignment divides by zero");
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    if (size > maximum - sizeof(void*) || alignment - 1 > maximum - sizeof(void*) - size)
        throw std::length_error("PKAllocAligned: block size overflows size_t");
    const auto blockSize = size + sizeof(void*) + alignment - 1;
    auto* original = static_cast<std::uint8_t*>(calloc_nid_postfix(1, blockSize));
    if (!original) return WmpErrOutOfMemory;
    auto correction = alignment - reinterpret_cast<std::uintptr_t>(original) % alignment;
    if (correction < sizeof(void*)) correction += alignment;
    if (correction < sizeof(void*)) {
        free_nid_postfix(original);
        throw std::invalid_argument("PKAllocAligned: the alignment leaves no room for the original pointer");
    }
    auto* aligned = original + correction;
    std::memcpy(aligned - sizeof(void*), &original, sizeof(original));
    *memory = aligned;
    return WmpErrSuccess;
}

std::int64_t APS5_VABI PKFreeAligned_nid_postfix(void** memory) {
    if (memory && *memory) {
        void* original = nullptr;
        std::memcpy(&original, static_cast<std::uint8_t*>(*memory) - sizeof(void*), sizeof(original));
        free_nid_postfix(original);
        *memory = nullptr;
    }
    return WmpErrSuccess;
}

std::int32_t APS5_VABI IDPEmpty_nid_postfix(void*) {
    return 0;
}

}
