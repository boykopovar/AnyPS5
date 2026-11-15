//*@@@+++@@@@******************************************************************
//
// Copyright © Microsoft Corp.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// • Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// • Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//*@@@---@@@@******************************************************************
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
    void* reserved;
    std::int64_t (APS5_VABI* release)(CodecFactory**);
};

struct FormatConverter {
    std::int64_t (APS5_VABI* initialize)(FormatConverter*, void*, char*, PixelGuid);
    std::int64_t (APS5_VABI* initializeConvert)(FormatConverter*, PixelGuid, char*, PixelGuid);
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
static_assert(sizeof(CodecFactory) == 40 && offsetof(CodecFactory, release) == 0x20);
static_assert(sizeof(FormatConverter) == 96 && offsetof(FormatConverter, decoder) == 72);

} // namespace Jxr

#endif
