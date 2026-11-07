#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int64_t Success = 0;
constexpr std::int64_t NotYetImplemented = -2;
constexpr std::int64_t AbstractMethod = -3;
constexpr std::int64_t OutOfMemory = -101;

Jxr::ImageEncode* Encoder(Jxr::ImageEncode* encoder, const char* function) {
    if (!encoder) throw std::invalid_argument(std::string(function) + " requires an encoder");
    return encoder;
}

std::int64_t APS5_VABI WritePixelsBandedBegin(Jxr::ImageEncode*, Jxr::Stream*) {
    return AbstractMethod;
}

std::int64_t APS5_VABI WritePixelsBanded(Jxr::ImageEncode*, std::uint32_t, std::uint8_t*, std::uint32_t, std::int32_t) {
    return AbstractMethod;
}

std::int64_t APS5_VABI WritePixelsBandedEnd(Jxr::ImageEncode*) {
    return AbstractMethod;
}

}

extern "C" {

std::int64_t APS5_VABI PKImageEncode_SetPixelFormat_nid_postfix(Jxr::ImageEncode* encoder, JxrGuestGuid format) {
    Encoder(encoder, __func__)->pixelFormat = format;
    return Success;
}

std::int64_t APS5_VABI PKImageEncode_SetSize_nid_postfix(Jxr::ImageEncode* encoder, std::int32_t width, std::int32_t height) {
    Encoder(encoder, __func__)->width = static_cast<std::uint32_t>(width);
    encoder->height = static_cast<std::uint32_t>(height);
    return Success;
}

std::int64_t APS5_VABI PKImageEncode_SetResolution_nid_postfix(Jxr::ImageEncode* encoder, float horizontal, float vertical) {
    Encoder(encoder, __func__)->resolutionX = horizontal;
    encoder->resolutionY = vertical;
    return Success;
}

std::int64_t APS5_VABI PKImageEncode_SetColorContext_nid_postfix(Jxr::ImageEncode*, const std::uint8_t*, std::uint32_t) {
    return NotYetImplemented;
}

std::int64_t APS5_VABI PKImageEncode_SetDescriptiveMetadata_nid_postfix(Jxr::ImageEncode*, const void*) {
    return NotYetImplemented;
}

std::int64_t APS5_VABI PKImageEncode_WritePixels_nid_postfix(Jxr::ImageEncode*, std::uint32_t, std::uint8_t*, std::uint32_t) {
    return AbstractMethod;
}

std::int64_t APS5_VABI PKImageEncode_Initialize_nid_postfix(Jxr::ImageEncode* encoder, Jxr::Stream* stream, void*, std::size_t) {
    Encoder(encoder, __func__)->stream = stream;
    encoder->pixelFormat = JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch;
    encoder->resolutionX = 96;
    encoder->resolutionY = 96;
    encoder->frames = 1;
    if (!stream || !stream->getPosition) throw std::invalid_argument("PKImageEncode_Initialize requires a stream with GetPos");
    return stream->getPosition(stream, &encoder->start);
}

std::int64_t APS5_VABI PKImageEncode_Release_nid_postfix(Jxr::ImageEncode** encoder) {
    if (!encoder || !*encoder || !(*encoder)->stream || !(*encoder)->stream->close)
        throw std::invalid_argument("PKImageEncode_Release requires an encoder with a stream");
    (*encoder)->stream->close(&(*encoder)->stream);
    return PKFree_nid_postfix(reinterpret_cast<void**>(encoder));
}

std::int64_t APS5_VABI PKImageEncode_Create_nid_postfix(Jxr::ImageEncode** encoder) {
    if (!encoder) throw std::invalid_argument("PKImageEncode_Create requires an output");
    *encoder = static_cast<Jxr::ImageEncode*>(calloc_nid_postfix(1, sizeof(Jxr::ImageEncode)));
    if (!*encoder) return OutOfMemory;
    auto* created = *encoder;
    created->initialize = &PKImageEncode_Initialize_nid_postfix;
    created->terminate = reinterpret_cast<decltype(created->terminate)>(&PKImageEncode_Terminate_nid_postfix);
    created->setPixelFormat = &PKImageEncode_SetPixelFormat_nid_postfix;
    created->setSize = &PKImageEncode_SetSize_nid_postfix;
    created->setResolution = &PKImageEncode_SetResolution_nid_postfix;
    created->setColorContext = &PKImageEncode_SetColorContext_nid_postfix;
    created->setDescriptiveMetadata = &PKImageEncode_SetDescriptiveMetadata_nid_postfix;
    created->writePixels = &PKImageEncode_WritePixels_nid_postfix;
    created->writePixelsBandedBegin = &WritePixelsBandedBegin;
    created->writePixelsBanded = &WritePixelsBanded;
    created->writePixelsBandedEnd = &WritePixelsBandedEnd;
    created->createNewFrame = reinterpret_cast<decltype(created->createNewFrame)>(&PKImageEncode_CreateNewFrame_nid_postfix);
    created->release = &PKImageEncode_Release_nid_postfix;
    created->wmp = 0;
    return Success;
}

}
