#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreDumpExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include <cstdint>

namespace WebCore {

struct ImageBufferBackend;

enum class ColorFormat : std::uint8_t {
    RGBA,
    BGRA
};

} // namespace WebCore

extern "C" {

void APS5_VABI _ZNK7WebCore18ImageBufferBackend15copyImagePixelsENS_22AlphaPremultiplicationENS_11ColorFormatEjPhS1_S2_jS3_RKNS_7IntSizeE(
    const WebCore::ImageBufferBackend* self,
    WebCore::AlphaPremultiplication srcAlphaFormat,
    WebCore::ColorFormat srcColorFormat,
    unsigned srcBytesPerRow,
    std::uint8_t* srcRows,
    WebCore::AlphaPremultiplication destAlphaFormat,
    WebCore::ColorFormat destColorFormat,
    unsigned destBytesPerRow,
    std::uint8_t* destRows,
    const WebCore::IntSize* size);

}
