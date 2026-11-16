#ifndef CORE_LIBS_PRX_LIBSCEJXR_IMAGEENCODE_HPP
#define CORE_LIBS_PRX_LIBSCEJXR_IMAGEENCODE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include "prx/libSceJxr/Stream.hpp"
#include "prx/libSceJxr/include/GuidExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

namespace Jxr {

struct ImageEncode {
    std::int64_t (APS5_VABI* initialize)(ImageEncode*, Stream*, void*, std::size_t);
    std::int64_t (APS5_VABI* terminate)(ImageEncode*);
    std::int64_t (APS5_VABI* setPixelFormat)(ImageEncode*, JxrGuestGuid);
    std::int64_t (APS5_VABI* setSize)(ImageEncode*, std::int32_t, std::int32_t);
    std::int64_t (APS5_VABI* setResolution)(ImageEncode*, float, float);
    std::int64_t (APS5_VABI* setColorContext)(ImageEncode*, const std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* setDescriptiveMetadata)(ImageEncode*, const void*);
    std::int64_t (APS5_VABI* writePixels)(ImageEncode*, std::uint32_t, std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* writeSource)(ImageEncode*, void*, void*);
    std::int64_t (APS5_VABI* writePixelsBandedBegin)(ImageEncode*, Stream*);
    std::int64_t (APS5_VABI* writePixelsBanded)(ImageEncode*, std::uint32_t, std::uint8_t*, std::uint32_t, std::int32_t);
    std::int64_t (APS5_VABI* writePixelsBandedEnd)(ImageEncode*);
    std::int64_t (APS5_VABI* transcode)(ImageEncode*, void*, void*);
    std::int64_t (APS5_VABI* createNewFrame)(ImageEncode*, void*, std::size_t);
    std::int64_t (APS5_VABI* release)(ImageEncode**);
    Stream* stream;
    std::size_t start;
    JxrGuestGuid pixelFormat;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t currentLine;
    float resolutionX;
    float resolutionY;
    std::uint32_t frames;
    std::array<std::uint8_t, 344> header;
    std::int32_t wmp;
    std::array<std::uint8_t, 66164> codec;
};

static_assert(sizeof(ImageEncode) == 66688);
static_assert(offsetof(ImageEncode, release) == 112 && offsetof(ImageEncode, stream) == 120);
static_assert(offsetof(ImageEncode, pixelFormat) == 136 && offsetof(ImageEncode, width) == 152);
static_assert(offsetof(ImageEncode, resolutionX) == 164 && offsetof(ImageEncode, frames) == 172);
static_assert(offsetof(ImageEncode, wmp) == 520);

} // namespace Jxr

#endif
