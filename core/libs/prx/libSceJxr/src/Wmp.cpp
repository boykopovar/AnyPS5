#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libSceJxr/src/EncoderMetadata.hpp"
#include "prx/libSceJxr/src/WmpBridge.h"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::mutex proxyMutex;
std::unordered_map<const void*, std::vector<std::unique_ptr<std::byte[]>>> proxies;

void Forget(const void* record) {
    std::lock_guard lock(proxyMutex);
    proxies.erase(record);
}

void Require(bool value, const char* function, const char* what) {
    if (!value) throw std::invalid_argument(std::string(function) + " requires " + what);
}

constexpr std::size_t GuestProbeChunkBytes = 4096;

struct GuestRect {
    std::int32_t x;
    std::int32_t y;
    std::int32_t width;
    std::int32_t height;
};

void RequireGuestAddressRange(const void* pointer, std::size_t size, const char* function) {
    if (size == 0) return;
    constexpr auto maxAddress = std::numeric_limits<std::uintptr_t>::max();
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    if (address == 0 || size > maxAddress - address) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

std::size_t CheckedFallbackPixelSpan(std::uint64_t lines, std::uint64_t stride, const char* function) {
    if (lines == 0) return 0;
    if (stride == 0 || lines > std::numeric_limits<std::size_t>::max() / stride)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return static_cast<std::size_t>(lines * stride);
}

std::size_t EncodePixelBufferSpan(const Jxr::ImageEncode* encoder, std::uint32_t lines, std::uint32_t stride,
                                  const char* function) {
    if (lines == 0) return 0;
    std::size_t span = 0;
    std::int32_t overflow = 0;
    if (JxrWmpEncodePixelSpan_nid_no_patch(encoder, lines, stride, &span, &overflow)) {
        if (overflow) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        return span;
    }
    return CheckedFallbackPixelSpan(lines, stride, function);
}

std::size_t DecodePixelBufferSpan(const void* decoder, const GuestRect& rect, std::uint32_t stride,
                                  const char* function) {
    if (rect.height < 0) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (rect.height == 0) return 0;
    std::size_t span = 0;
    std::int32_t overflow = 0;
    if (JxrWmpDecodePixelSpan_nid_no_patch(decoder, rect.height, stride, &span, &overflow)) {
        if (overflow) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        return span;
    }
    return CheckedFallbackPixelSpan(static_cast<std::uint32_t>(rect.height), stride, function);
}

void CopyInGuestBytes(void* destination, const void* source, std::size_t size, const char* function) {
    if (size == 0) return;
    RequireGuestAddressRange(source, size, function);
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, size);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (size == 0) return;
    RequireGuestAddressRange(pointer, size, function);
    std::array<std::byte, GuestProbeChunkBytes> scratch{};
    const auto* cursor = static_cast<const std::byte*>(pointer);
    std::size_t remaining = size;
    while (remaining > 0) {
        const std::size_t chunk = std::min(remaining, scratch.size());
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(scratch.data(), cursor, chunk);
        errno = savedErrno;
        if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        cursor += chunk;
        remaining -= chunk;
    }
}

void RequireWritableGuestBytes(void* pointer, std::size_t size, const char* function) {
    if (size == 0) return;
    RequireGuestAddressRange(pointer, size, function);
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(pointer, size);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template <typename TValue>
TValue ReadGuestOutputSlot(const TValue* pointer, const char* function) {
    TValue value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, pointer, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template <typename TValue>
void RequireWritableGuestSlot(TValue* pointer, const char* function) {
    static_cast<void>(ReadGuestOutputSlot(pointer, function));
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(pointer, sizeof(TValue));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template <typename TValue>
bool WriteGuestOutputSlot(TValue* pointer, const TValue& value) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(pointer, &value, sizeof(value));
    errno = savedErrno;
    return ok;
}

std::int64_t Checked(std::int64_t result, std::int32_t refusal, const char* function) {
    switch (refusal) {
    case JXR_WMP_ACCEPTED:
        return result;
    case JXR_WMP_REFUSE_STREAM:
        throw std::invalid_argument(std::string(function) + " requires an initialized stream");
    case JXR_WMP_REFUSE_STATE:
        throw std::logic_error(std::string(function) + " was called in an encode state that jxrlib asserts against");
    case JXR_WMP_REFUSE_ALPHA_MODE:
        throw std::invalid_argument(std::string(function) + " requires an alpha mode of at most 3");
    case JXR_WMP_REFUSE_CALLBACK:
        throw std::invalid_argument(std::string(function) + " requires the encoder and decoder callbacks it calls");
    default:
        throw std::logic_error(std::string(function) + " was refused for an unexpected reason");
    }
}

std::int64_t Supported(std::int64_t result, std::int32_t refusal, const char* function) {
    if (refusal == JXR_WMP_REFUSE_FORMAT || refusal == JXR_WMP_REFUSE_WIDE_TEXT) NotImplemented_nid_no_patch(function);
    return Checked(result, refusal, function);
}

std::int64_t APS5_VABI EncodeInitialize(Jxr::ImageEncode* encoder, Jxr::Stream* stream, void* parameters,
                                        std::size_t size) {
    Require(encoder != nullptr, "PKImageEncode_Initialize_WMP", "an encoder");
    const auto expectedSize = JxrWmpEncodeParameterSize_nid_no_patch();
    Require(parameters != nullptr || size != expectedSize, "PKImageEncode_Initialize_WMP", "codec parameters");
    if (size == expectedSize) {
        std::vector<std::byte> localParameters(size);
        CopyInGuestBytes(localParameters.data(), parameters, size, "PKImageEncode_Initialize_WMP");
        return JxrWmpEncodeInitialize_nid_no_patch(encoder, stream, localParameters.data(), size);
    }
    return JxrWmpEncodeInitialize_nid_no_patch(encoder, stream, nullptr, size);
}

std::int64_t APS5_VABI EncodeTerminate(Jxr::ImageEncode* encoder) {
    return JxrWmpEncodeTerminate_nid_no_patch(encoder);
}

std::int64_t APS5_VABI EncodeSetColorContext(Jxr::ImageEncode* encoder, const std::uint8_t* context,
                                             std::uint32_t size) {
    return Jxr::SetImageEncodeMetadata(encoder, context, size, offsetof(Jxr::ImageEncodeMetadataPrefix, colorContext),
                                       offsetof(Jxr::ImageEncodeMetadataPrefix, colorContextSize));
}

std::int64_t APS5_VABI EncodeSetDescriptiveMetadata(Jxr::ImageEncode* encoder, const void* metadata) {
    Require(encoder != nullptr && metadata != nullptr, "PKImageEncode_SetDescriptiveMetadata_WMP",
            "an encoder and metadata");
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpEncodeSetDescriptiveMetadata_nid_no_patch(encoder, metadata, &refusal);
    return Supported(result, refusal, "PKImageEncode_SetDescriptiveMetadata_WMP");
}

std::int64_t APS5_VABI EncodeWritePixels(Jxr::ImageEncode* encoder, std::uint32_t lines, std::uint8_t* pixels,
                                         std::uint32_t stride) {
    Require(encoder != nullptr, "PKImageEncode_WritePixels_WMP", "an encoder");
    RequireReadableGuestBytes(pixels, EncodePixelBufferSpan(encoder, lines, stride, "PKImageEncode_WritePixels_WMP"),
                              "PKImageEncode_WritePixels_WMP");
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpEncodeWritePixels_nid_no_patch(encoder, lines, pixels, stride, &refusal);
    return Supported(result, refusal, "PKImageEncode_WritePixels_WMP");
}

std::int64_t APS5_VABI EncodeBandedBegin(Jxr::ImageEncode* encoder, Jxr::Stream* temporary) {
    Require(encoder != nullptr, "PKImageEncode_WritePixelsBandedBegin_WMP", "an encoder");
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpEncodeBandedBegin_nid_no_patch(encoder, temporary, &refusal);
    return Checked(result, refusal, "PKImageEncode_WritePixelsBandedBegin_WMP");
}

std::int64_t APS5_VABI EncodeBanded(Jxr::ImageEncode* encoder, std::uint32_t lines, std::uint8_t* pixels,
                                    std::uint32_t stride, std::int32_t last) {
    Require(encoder != nullptr, "PKImageEncode_WritePixelsBanded_WMP", "an encoder");
    RequireReadableGuestBytes(pixels,
                              EncodePixelBufferSpan(encoder, lines, stride, "PKImageEncode_WritePixelsBanded_WMP"),
                              "PKImageEncode_WritePixelsBanded_WMP");
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpEncodeBanded_nid_no_patch(encoder, lines, pixels, stride, last, &refusal);
    return Supported(result, refusal, "PKImageEncode_WritePixelsBanded_WMP");
}

std::int64_t APS5_VABI EncodeBandedEnd(Jxr::ImageEncode* encoder) {
    Require(encoder != nullptr, "PKImageEncode_WritePixelsBandedEnd_WMP", "an encoder");
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpEncodeBandedEnd_nid_no_patch(encoder, &refusal);
    return Checked(result, refusal, "PKImageEncode_WritePixelsBandedEnd_WMP");
}

std::int64_t APS5_VABI EncodeTranscode(Jxr::ImageEncode* encoder, void* decoder, void* parameters) {
    Require(encoder != nullptr && decoder != nullptr && parameters != nullptr, "PKImageEncode_Transcode_WMP",
            "an encoder, a decoder and transcoding parameters");
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpEncodeTranscode_nid_no_patch(encoder, decoder, parameters, &refusal);
    return Supported(result, refusal, "PKImageEncode_Transcode_WMP");
}

std::int64_t APS5_VABI EncodeCreateNewFrame(Jxr::ImageEncode* encoder, void* parameters, std::size_t size) {
    return JxrWmpEncodeCreateNewFrame_nid_no_patch(encoder, parameters, size);
}

std::int64_t APS5_VABI EncodeRelease(Jxr::ImageEncode** encoder) {
    Require(encoder != nullptr, "PKImageEncode_Release_WMP", "an encoder");
    auto* current = ReadGuestOutputSlot(encoder, "PKImageEncode_Release_WMP");
    RequireWritableGuestSlot(encoder, "PKImageEncode_Release_WMP");
    Require(current != nullptr, "PKImageEncode_Release_WMP", "an encoder");
    void* local = current;
    const void* record = local;
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpEncodeRelease_nid_no_patch(&local, &refusal);
    if (refusal == JXR_WMP_ACCEPTED) {
        Forget(record);
        auto* updated = static_cast<Jxr::ImageEncode*>(local);
        if (!WriteGuestOutputSlot(encoder, updated)) NotImplementedAt_nid_no_patch("PKImageEncode_Release_WMP", __FILE__, __LINE__);
    }
    return Checked(result, refusal, "PKImageEncode_Release_WMP");
}

std::int64_t APS5_VABI DecodeInitialize(void* decoder, void* stream) {
    Require(decoder != nullptr && stream != nullptr, "PKImageDecode_Initialize_WMP", "a decoder and a stream");
    return JxrWmpDecodeInitialize_nid_no_patch(decoder, stream);
}

std::int64_t APS5_VABI DecodeGetSize(void* decoder, std::int32_t* width, std::int32_t* height) {
    Require(decoder != nullptr && width != nullptr && height != nullptr, "PKImageDecode_GetSize_WMP",
            "a decoder and outputs");
    RequireWritableGuestSlot(width, "PKImageDecode_GetSize_WMP");
    RequireWritableGuestSlot(height, "PKImageDecode_GetSize_WMP");
    std::int32_t localWidth = 0;
    std::int32_t localHeight = 0;
    const auto result = JxrWmpDecodeGetSize_nid_no_patch(decoder, &localWidth, &localHeight);
    if (!WriteGuestOutputSlot(width, localWidth) || !WriteGuestOutputSlot(height, localHeight))
        NotImplementedAt_nid_no_patch("PKImageDecode_GetSize_WMP", __FILE__, __LINE__);
    return result;
}

std::int64_t APS5_VABI DecodeGetRawStream(void* decoder, void** stream) {
    Require(decoder != nullptr && stream != nullptr, "PKImageDecode_GetRawStream_WMP", "a decoder and an output");
    RequireWritableGuestSlot(stream, "PKImageDecode_GetRawStream_WMP");
    void* localStream = nullptr;
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpDecodeGetRawStream_nid_no_patch(decoder, &localStream, &refusal);
    const auto checked = Checked(result, refusal, "PKImageDecode_GetRawStream_WMP");
    if (!WriteGuestOutputSlot(stream, localStream))
        NotImplementedAt_nid_no_patch("PKImageDecode_GetRawStream_WMP", __FILE__, __LINE__);
    return checked;
}

std::int64_t APS5_VABI DecodeCopy(void* decoder, const void* rectangle, std::uint8_t* pixels, std::uint32_t stride) {
    Require(decoder != nullptr && rectangle != nullptr, "PKImageDecode_Copy_WMP", "a decoder and a rectangle");
    const auto localRect = ReadGuestOutputSlot(static_cast<const GuestRect*>(rectangle), "PKImageDecode_Copy_WMP");
    RequireWritableGuestBytes(pixels, DecodePixelBufferSpan(decoder, localRect, stride, "PKImageDecode_Copy_WMP"),
                              "PKImageDecode_Copy_WMP");
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpDecodeCopy_nid_no_patch(decoder, &localRect, pixels, stride, &refusal);
    return Checked(result, refusal, "PKImageDecode_Copy_WMP");
}

std::int64_t APS5_VABI DecodeGetColorContext(void* decoder, std::uint8_t* buffer, std::uint32_t* size) {
    Require(decoder != nullptr && size != nullptr, "PKImageDecode_GetColorContext_WMP", "a decoder and a size");
    auto localSize = ReadGuestOutputSlot(size, "PKImageDecode_GetColorContext_WMP");
    RequireWritableGuestSlot(size, "PKImageDecode_GetColorContext_WMP");
    if (buffer != nullptr && localSize > 0)
        RequireWritableGuestBytes(buffer, localSize, "PKImageDecode_GetColorContext_WMP");
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpDecodeGetColorContext_nid_no_patch(decoder, buffer, &localSize, &refusal);
    const auto checked = Checked(result, refusal, "PKImageDecode_GetColorContext_WMP");
    if (!WriteGuestOutputSlot(size, localSize))
        NotImplementedAt_nid_no_patch("PKImageDecode_GetColorContext_WMP", __FILE__, __LINE__);
    return checked;
}

std::int64_t APS5_VABI DecodeGetDescriptiveMetadata(void* decoder, void* metadata) {
    Require(decoder != nullptr && metadata != nullptr, "PKImageDecode_GetDescriptiveMetadata_WMP",
            "a decoder and an output");
    return JxrWmpDecodeGetDescriptiveMetadata_nid_no_patch(decoder, metadata);
}

std::int64_t APS5_VABI DecodeRelease(void** decoder) {
    if (decoder == nullptr) return 0;
    void* local = ReadGuestOutputSlot(decoder, "PKImageDecode_Release_WMP");
    RequireWritableGuestSlot(decoder, "PKImageDecode_Release_WMP");
    Require(local != nullptr, "PKImageDecode_Release_WMP", "a decoder");
    const void* record = local;
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpDecodeRelease_nid_no_patch(&local, &refusal);
    if (refusal == JXR_WMP_ACCEPTED) {
        Forget(record);
        if (!WriteGuestOutputSlot(decoder, local)) NotImplementedAt_nid_no_patch("PKImageDecode_Release_WMP", __FILE__, __LINE__);
    }
    return Checked(result, refusal, "PKImageDecode_Release_WMP");
}

}

extern "C" {

void* JxrWmpRetainProxy_nid_no_patch(void* record, std::size_t size) {
    auto block = std::make_unique<std::byte[]>(size);
    auto* proxy = block.get();
    std::lock_guard lock(proxyMutex);
    proxies[record].push_back(std::move(block));
    return proxy;
}

void JxrWmpMissingCallback_nid_no_patch(const char* name) {
    throw std::invalid_argument(std::string("JXR WMP stream has no ") + name + " callback");
}

std::int64_t APS5_VABI PKImageEncode_Create_WMP_nid_postfix(Jxr::ImageEncode** encoder) {
    if (!encoder) throw std::invalid_argument("PKImageEncode_Create requires an output");
    RequireWritableGuestSlot(encoder, __func__);
    Jxr::ImageEncode* created = nullptr;
    const auto result = PKImageEncode_Create_nid_postfix(&created);
    if (result < 0) {
        if (!WriteGuestOutputSlot(encoder, created)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return result;
    }
    Forget(created);
    created->initialize = &EncodeInitialize;
    created->terminate = &EncodeTerminate;
    created->setColorContext = &EncodeSetColorContext;
    created->setDescriptiveMetadata = &EncodeSetDescriptiveMetadata;
    created->writePixels = &EncodeWritePixels;
    created->writePixelsBandedBegin = &EncodeBandedBegin;
    created->writePixelsBanded = &EncodeBanded;
    created->writePixelsBandedEnd = &EncodeBandedEnd;
    created->transcode = &EncodeTranscode;
    created->createNewFrame = &EncodeCreateNewFrame;
    created->release = &EncodeRelease;
    created->wmp = 1;
    if (!WriteGuestOutputSlot(encoder, created)) {
        void* released = created;
        PKFree_nid_postfix(&released);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    return result;
}

std::int64_t APS5_VABI PKImageDecode_Create_WMP_nid_postfix(void** decoder) {
    static const JxrWmpDecodeSlots slots{
        DecodeInitialize,
        DecodeGetSize,
        DecodeGetRawStream,
        DecodeCopy,
        DecodeGetColorContext,
        DecodeGetDescriptiveMetadata,
        DecodeRelease};
    if (!decoder) throw std::invalid_argument("PKImageDecode_Create requires an output");
    RequireWritableGuestSlot(decoder, __func__);
    void* created = nullptr;
    const auto result = PKImageDecode_Create_nid_postfix(&created);
    if (result < 0) {
        if (!WriteGuestOutputSlot(decoder, created)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return result;
    }
    Forget(created);
    JxrWmpDecodeInstall_nid_no_patch(created, &slots);
    if (!WriteGuestOutputSlot(decoder, created)) {
        PKFree_nid_postfix(&created);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    return result;
}

}
