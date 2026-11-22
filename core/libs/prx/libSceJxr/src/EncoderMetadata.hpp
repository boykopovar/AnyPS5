#pragma once

#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace Jxr {

inline constexpr std::int64_t WmpErrOutOfSequence = -109;

struct ImageEncodeMetadataPrefix {
    void* callbacks[15];
    void* stream;
    std::size_t startOffset;
    std::byte pixelFormat[16];
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t currentLine;
    float resolutionX;
    float resolutionY;
    std::uint32_t frameCount;
    std::int32_t headerDone;
    std::size_t pixelOffset;
    std::size_t pixelSize;
    void* colorContext;
    std::uint32_t colorContextSize;
    std::uint8_t* exifMetadata;
    std::uint32_t exifMetadataByteCount;
    std::uint8_t* gpsInfoMetadata;
    std::uint32_t gpsInfoMetadataByteCount;
    std::uint8_t* iptcNaaMetadata;
    std::uint32_t iptcNaaMetadataByteCount;
    std::uint8_t* xmpMetadata;
    std::uint32_t xmpMetadataByteCount;
    std::uint8_t* photoshopMetadata;
    std::uint32_t photoshopMetadataByteCount;
};

static_assert(offsetof(ImageEncodeMetadataPrefix, stream) == 120);
static_assert(offsetof(ImageEncodeMetadataPrefix, startOffset) == 128);
static_assert(offsetof(ImageEncodeMetadataPrefix, pixelFormat) == 136);
static_assert(offsetof(ImageEncodeMetadataPrefix, width) == 152);
static_assert(offsetof(ImageEncodeMetadataPrefix, headerDone) == 176);
static_assert(offsetof(ImageEncodeMetadataPrefix, exifMetadata) == 216);
static_assert(offsetof(ImageEncodeMetadataPrefix, exifMetadataByteCount) == 224);
static_assert(offsetof(ImageEncodeMetadataPrefix, gpsInfoMetadata) == 232);
static_assert(offsetof(ImageEncodeMetadataPrefix, gpsInfoMetadataByteCount) == 240);
static_assert(offsetof(ImageEncodeMetadataPrefix, iptcNaaMetadata) == 248);
static_assert(offsetof(ImageEncodeMetadataPrefix, iptcNaaMetadataByteCount) == 256);
static_assert(offsetof(ImageEncodeMetadataPrefix, xmpMetadata) == 264);
static_assert(offsetof(ImageEncodeMetadataPrefix, xmpMetadataByteCount) == 272);
static_assert(offsetof(ImageEncodeMetadataPrefix, photoshopMetadata) == 280);
static_assert(offsetof(ImageEncodeMetadataPrefix, photoshopMetadataByteCount) == 288);
static_assert(sizeof(ImageEncodeMetadataPrefix) == 296);

inline std::uintptr_t MetadataGuestField(const void* base, std::size_t offset, std::size_t width, const char* function) {
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    constexpr auto maximum = std::numeric_limits<std::uintptr_t>::max();
    if (address == 0 || offset > maximum - address || width > maximum - (address + offset))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return address + offset;
}

template <typename TValue>
TValue CopyInGuestField(std::uintptr_t address, const char* function) {
    TValue value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address), sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template <typename TValue>
void CopyOutGuestField(std::uintptr_t address, const TValue& value, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(address), &value, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

inline void RequireWritableGuestField(std::uintptr_t address, std::size_t width, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(reinterpret_cast<void*>(address), width);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

inline bool CopyInGuestBytes(void* destination, const void* source, std::size_t bytes) {
    if (bytes == 0) return true;
    const auto address = reinterpret_cast<std::uintptr_t>(source);
    if (address == 0 || bytes > std::numeric_limits<std::uintptr_t>::max() - address) return false;
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

inline std::int64_t SetImageEncodeMetadata(void* encoder, const std::uint8_t* metadata, std::uint32_t size,
                                          std::size_t dataOffset, std::size_t sizeOffset) {
    if (!encoder) throw std::invalid_argument("metadata setter requires an encoder");
    const auto headerDoneAddress = MetadataGuestField(encoder, offsetof(ImageEncodeMetadataPrefix, headerDone), sizeof(std::int32_t), __func__);
    if (CopyInGuestField<std::int32_t>(headerDoneAddress, __func__) != 0) return WmpErrOutOfSequence;

    const auto storedAddress = MetadataGuestField(encoder, dataOffset, sizeof(std::uint8_t*), __func__);
    const auto storedSizeAddress = MetadataGuestField(encoder, sizeOffset, sizeof(std::uint32_t), __func__);
    auto* previousPointer = CopyInGuestField<std::uint8_t*>(storedAddress, __func__);
    static_cast<void>(CopyInGuestField<std::uint32_t>(storedSizeAddress, __func__));
    RequireWritableGuestField(storedAddress, sizeof(std::uint8_t*), __func__);
    RequireWritableGuestField(storedSizeAddress, sizeof(std::uint32_t), __func__);

    void* previous = previousPointer;
    PKFree_nid_postfix(&previous);
    auto* clearedPointer = static_cast<std::uint8_t*>(previous);
    const std::uint32_t zeroSize = 0;
    CopyOutGuestField(storedAddress, clearedPointer, __func__);
    CopyOutGuestField(storedSizeAddress, zeroSize, __func__);

    void* allocated = clearedPointer;
    const auto allocationResult = PKAlloc_nid_postfix(&allocated, size);
    auto* storedPointer = static_cast<std::uint8_t*>(allocated);
    CopyOutGuestField(storedAddress, storedPointer, __func__);
    if (allocationResult < 0) return allocationResult;
    if (size != 0 && metadata == nullptr) throw std::invalid_argument("metadata setter requires input bytes");
    if (size != 0 && !CopyInGuestBytes(storedPointer, metadata, size)) {
        PKFree_nid_postfix(&allocated);
        auto* nullPointer = static_cast<std::uint8_t*>(allocated);
        CopyOutGuestField(storedAddress, nullPointer, __func__);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    CopyOutGuestField(storedSizeAddress, size, __func__);
    return allocationResult;
}

}
