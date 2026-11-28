#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestPtr.hpp"

#include <cstddef>
#include <cerrno>
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

template <typename Value>
void StoreField(Jxr::ImageEncode* encoder, std::size_t offset, const Value& value, const char* function) {
    if (!encoder) throw std::invalid_argument(std::string(function) + " requires an encoder");
    const int savedErrno = errno;
    const bool written = GuestBytes(reinterpret_cast<const std::byte*>(encoder) + offset).CopyOut(&value, sizeof(Value));
    errno = savedErrno;
    if (!written) throw std::invalid_argument(std::string(function) + " cannot write the encoder");
}

template <typename Value>
Value LoadValue(const void* address, const char* function) {
    Value value{};
    const int savedErrno = errno;
    const bool read = GuestPtr<Value>(address).CopyIn(value);
    errno = savedErrno;
    if (!read) throw std::invalid_argument(std::string(function) + " cannot read guest memory");
    return value;
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
    const auto w = static_cast<std::uint32_t>(width);
    const auto h = static_cast<std::uint32_t>(height);
    StoreField(encoder, offsetof(Jxr::ImageEncode, width), w, __func__);
    StoreField(encoder, offsetof(Jxr::ImageEncode, height), h, __func__);
    return Success;
}

std::int64_t APS5_VABI PKImageEncode_SetResolution_nid_postfix(Jxr::ImageEncode* encoder, float horizontal, float vertical) {
    StoreField(encoder, offsetof(Jxr::ImageEncode, resolutionX), horizontal, __func__);
    StoreField(encoder, offsetof(Jxr::ImageEncode, resolutionY), vertical, __func__);
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
    Encoder(encoder, __func__);
    StoreField(encoder, offsetof(Jxr::ImageEncode, stream), stream, __func__);
    const JxrGuestGuid format = JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch;
    StoreField(encoder, offsetof(Jxr::ImageEncode, pixelFormat), format, __func__);
    StoreField(encoder, offsetof(Jxr::ImageEncode, resolutionX), 96.0f, __func__);
    StoreField(encoder, offsetof(Jxr::ImageEncode, resolutionY), 96.0f, __func__);
    StoreField(encoder, offsetof(Jxr::ImageEncode, frames), std::uint32_t{1}, __func__);
    if (!stream) throw std::invalid_argument("PKImageEncode_Initialize requires a stream with GetPos");
    const auto getPosition = LoadValue<decltype(Jxr::Stream::getPosition)>(
        reinterpret_cast<const std::byte*>(stream) + offsetof(Jxr::Stream, getPosition), __func__);
    if (!getPosition) throw std::invalid_argument("PKImageEncode_Initialize requires a stream with GetPos");
    std::size_t start = 0;
    const std::int64_t result = getPosition(stream, &start);
    StoreField(encoder, offsetof(Jxr::ImageEncode, start), start, __func__);
    return result;
}

std::int64_t APS5_VABI PKImageEncode_Release_nid_postfix(Jxr::ImageEncode** encoder) {
    if (!encoder) throw std::invalid_argument("PKImageEncode_Release requires an encoder with a stream");
    auto* current = LoadValue<Jxr::ImageEncode*>(encoder, __func__);
    if (!current) throw std::invalid_argument("PKImageEncode_Release requires an encoder with a stream");
    const auto stream = LoadValue<Jxr::Stream*>(reinterpret_cast<const std::byte*>(current) + offsetof(Jxr::ImageEncode, stream), __func__);
    if (!stream) throw std::invalid_argument("PKImageEncode_Release requires an encoder with a stream");
    const auto close = LoadValue<decltype(Jxr::Stream::close)>(reinterpret_cast<const std::byte*>(stream) + offsetof(Jxr::Stream, close), __func__);
    if (!close) throw std::invalid_argument("PKImageEncode_Release requires an encoder with a stream");
    Jxr::Stream* closing = stream;
    close(&closing);
    StoreField(current, offsetof(Jxr::ImageEncode, stream), closing, __func__);
    return PKFree_nid_postfix(reinterpret_cast<void**>(encoder));
}

std::int64_t APS5_VABI PKImageEncode_Create_nid_postfix(Jxr::ImageEncode** encoder) {
    if (!encoder) throw std::invalid_argument("PKImageEncode_Create requires an output");
    auto* created = static_cast<Jxr::ImageEncode*>(calloc_nid_postfix(1, sizeof(Jxr::ImageEncode)));
    const int savedErrno = errno;
    if (!created) {
        const bool cleared = GuestPtr<Jxr::ImageEncode*>(encoder).CopyOut(created);
        errno = savedErrno;
        if (!cleared) throw std::invalid_argument("PKImageEncode_Create cannot write the output");
        return OutOfMemory;
    }
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
    const bool published = GuestPtr<Jxr::ImageEncode*>(encoder).CopyOut(created);
    errno = savedErrno;
    if (!published) {
        free_nid_postfix(created);
        throw std::invalid_argument("PKImageEncode_Create cannot write the output");
    }
    return Success;
}

}
