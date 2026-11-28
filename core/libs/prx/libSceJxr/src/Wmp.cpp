#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libSceJxr/src/EncoderMetadata.hpp"
#include "prx/libSceJxr/src/WmpBridge.h"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
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
    Require(parameters != nullptr || size != JxrWmpEncodeParameterSize_nid_no_patch(),
            "PKImageEncode_Initialize_WMP", "codec parameters");
    return JxrWmpEncodeInitialize_nid_no_patch(encoder, stream, parameters, size);
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
    Require(encoder != nullptr && *encoder != nullptr, "PKImageEncode_Release_WMP", "an encoder");
    const void* record = *encoder;
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpEncodeRelease_nid_no_patch(reinterpret_cast<void**>(encoder), &refusal);
    if (refusal == JXR_WMP_ACCEPTED) Forget(record);
    return Checked(result, refusal, "PKImageEncode_Release_WMP");
}

std::int64_t APS5_VABI DecodeInitialize(void* decoder, void* stream) {
    Require(decoder != nullptr && stream != nullptr, "PKImageDecode_Initialize_WMP", "a decoder and a stream");
    return JxrWmpDecodeInitialize_nid_no_patch(decoder, stream);
}

std::int64_t APS5_VABI DecodeGetSize(void* decoder, std::int32_t* width, std::int32_t* height) {
    Require(decoder != nullptr && width != nullptr && height != nullptr, "PKImageDecode_GetSize_WMP",
            "a decoder and outputs");
    return JxrWmpDecodeGetSize_nid_no_patch(decoder, width, height);
}

std::int64_t APS5_VABI DecodeGetRawStream(void* decoder, void** stream) {
    Require(decoder != nullptr && stream != nullptr, "PKImageDecode_GetRawStream_WMP", "a decoder and an output");
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpDecodeGetRawStream_nid_no_patch(decoder, stream, &refusal);
    return Checked(result, refusal, "PKImageDecode_GetRawStream_WMP");
}

std::int64_t APS5_VABI DecodeCopy(void* decoder, const void* rectangle, std::uint8_t* pixels, std::uint32_t stride) {
    Require(decoder != nullptr && rectangle != nullptr, "PKImageDecode_Copy_WMP", "a decoder and a rectangle");
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpDecodeCopy_nid_no_patch(decoder, rectangle, pixels, stride, &refusal);
    return Checked(result, refusal, "PKImageDecode_Copy_WMP");
}

std::int64_t APS5_VABI DecodeGetColorContext(void* decoder, std::uint8_t* buffer, std::uint32_t* size) {
    Require(decoder != nullptr && size != nullptr, "PKImageDecode_GetColorContext_WMP", "a decoder and a size");
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpDecodeGetColorContext_nid_no_patch(decoder, buffer, size, &refusal);
    return Checked(result, refusal, "PKImageDecode_GetColorContext_WMP");
}

std::int64_t APS5_VABI DecodeGetDescriptiveMetadata(void* decoder, void* metadata) {
    Require(decoder != nullptr && metadata != nullptr, "PKImageDecode_GetDescriptiveMetadata_WMP",
            "a decoder and an output");
    return JxrWmpDecodeGetDescriptiveMetadata_nid_no_patch(decoder, metadata);
}

std::int64_t APS5_VABI DecodeRelease(void** decoder) {
    if (decoder == nullptr) return 0;
    Require(*decoder != nullptr, "PKImageDecode_Release_WMP", "a decoder");
    const void* record = *decoder;
    std::int32_t refusal = JXR_WMP_ACCEPTED;
    const auto result = JxrWmpDecodeRelease_nid_no_patch(decoder, &refusal);
    if (refusal == JXR_WMP_ACCEPTED) Forget(record);
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
    const auto result = PKImageEncode_Create_nid_postfix(encoder);
    if (result < 0) return result;
    auto* created = *encoder;
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
    const auto result = PKImageDecode_Create_nid_postfix(decoder);
    if (result < 0) return result;
    Forget(*decoder);
    JxrWmpDecodeInstall_nid_no_patch(*decoder, &slots);
    return result;
}

}
