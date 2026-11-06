#pragma once

#include <cstddef>
#include <cstdint>

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

}
