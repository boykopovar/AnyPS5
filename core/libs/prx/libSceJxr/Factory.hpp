#ifndef CORE_LIBS_PRX_LIBSCEJXR_FACTORY_HPP
#define CORE_LIBS_PRX_LIBSCEJXR_FACTORY_HPP

#include <cstddef>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJxr/Stream.hpp"

namespace Jxr {

struct PixelGuid {
    std::uint8_t bytes[16];
};

struct PlainStream {
    std::int64_t (APS5_VABI* initializeFromFilename)(const char*, std::uint64_t);
    std::int64_t (APS5_VABI* release)();
    void* file;
};

struct Factory {
    std::int64_t (APS5_VABI* createStream)(PlainStream**);
    std::int64_t (APS5_VABI* createStreamFromFilename)(Stream**, const char*, const char*);
    std::int64_t (APS5_VABI* createStreamFromMemory)(Stream**, void*, std::size_t);
    std::int64_t (APS5_VABI* release)(Factory**);
};

struct CodecFactory {
    std::int64_t (APS5_VABI* createCodec)(const std::uint32_t*, void**);
    std::int64_t (APS5_VABI* createDecoderFromFile)(const char*, void**);
    std::int64_t (APS5_VABI* createFormatConverter)(void**);
    std::int64_t (APS5_VABI* release)(CodecFactory**);
};

struct FormatConverter {
    std::int64_t (APS5_VABI* initialize)(FormatConverter*, void*, char*, PixelGuid);
    std::int64_t (APS5_VABI* initializeConvert)(FormatConverter*, const PixelGuid*, char*, PixelGuid);
    std::int64_t (APS5_VABI* getPixelFormat)(FormatConverter*, PixelGuid*);
    std::int64_t (APS5_VABI* getSourcePixelFormat)(FormatConverter*, PixelGuid*);
    std::int64_t (APS5_VABI* getSize)(FormatConverter*, std::int32_t*, std::int32_t*);
    std::int64_t (APS5_VABI* getResolution)(FormatConverter*, float*, float*);
    std::int64_t (APS5_VABI* copy)(FormatConverter*, const void*, std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* convert)(FormatConverter*, const void*, std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* release)(FormatConverter**);
    void* decoder;
    PixelGuid pixelFormat;
};

static_assert(sizeof(PlainStream) == 24);
static_assert(sizeof(Factory) == 32);
static_assert(sizeof(CodecFactory) == 32);
static_assert(sizeof(FormatConverter) == 96 && offsetof(FormatConverter, decoder) == 72);

} // namespace Jxr

#endif
