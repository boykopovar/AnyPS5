#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libSceJxr/include/GuidExports.hpp"
#include "prx/libSceJxr/src/TranscodeBridge.h"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Err = std::int64_t;
using GetPixelFormat = Err(APS5_VABI*)(void*, JxrGuestGuid*);
using CopyPixels = Err(APS5_VABI*)(void*, const void*, std::uint8_t*, std::uint32_t);
using WritePixels = Err(APS5_VABI*)(void*, std::uint32_t, std::uint8_t*, std::uint32_t);
using Transcode = Err(APS5_VABI*)(void*, void*, void*);
constexpr Err UnsupportedFormat = -106;

struct Rect {
    std::int32_t x;
    std::int32_t y;
    std::int32_t width;
    std::int32_t height;
};

struct TranscodingParam {
    std::size_t leftX;
    std::size_t width;
    std::size_t topY;
    std::size_t height;
    std::int32_t bitstreamFormat;
    std::uint8_t alphaMode;
    std::uint8_t padding[3];
    std::int32_t subband;
    std::int32_t orientation;
    std::int32_t ignoreOverlap;
};

static_assert(sizeof(Rect) == 16);
static_assert(sizeof(TranscodingParam) == 56);
static_assert(offsetof(TranscodingParam, bitstreamFormat) == 32);
static_assert(offsetof(TranscodingParam, alphaMode) == 36);
static_assert(offsetof(TranscodingParam, subband) == 40);
static_assert(offsetof(TranscodingParam, orientation) == 44);
static_assert(offsetof(TranscodingParam, ignoreOverlap) == 48);

template <typename T>
void CopyFromGuest(T& value, const void* address, const char* function) {
    if (address == nullptr || !GuestCopyIn_nid_no_patch(&value, address, sizeof(value)))
        throw std::invalid_argument(std::string(function) + ": unreadable guest record");
}

std::vector<std::byte> Record(const void* address, std::size_t size, const char* function) {
    if (address == nullptr) throw std::invalid_argument(std::string(function) + ": null guest record");
    std::vector<std::byte> copy(size);
    if (!GuestCopyIn_nid_no_patch(copy.data(), address, copy.size()))
        throw std::invalid_argument(std::string(function) + ": unreadable guest record");
    return copy;
}

JxrTranscodeEncoderFields EncoderFields(const void* encoder, const char* function) {
    auto record = Record(encoder, JxrTranscodeEncoderSize_nid_no_patch(), function);
    JxrTranscodeEncoderFields fields{};
    JxrReadTranscodeEncoder_nid_no_patch(record.data(), &fields);
    return fields;
}

JxrTranscodeConverterFields ConverterFields(const void* converter, const char* function) {
    auto record = Record(converter, JxrTranscodeConverterSize_nid_no_patch(), function);
    JxrTranscodeConverterFields fields{};
    JxrReadTranscodeConverter_nid_no_patch(record.data(), &fields);
    return fields;
}

JxrTranscodeDecoderFields DecoderFields(const void* decoder, const char* function) {
    auto record = Record(decoder, JxrTranscodeDecoderSize_nid_no_patch(), function);
    JxrTranscodeDecoderFields fields{};
    JxrReadTranscodeDecoder_nid_no_patch(record.data(), &fields);
    return fields;
}

void ValidateCallback(std::uintptr_t address, const char* function) {
    if (address == 0) throw std::invalid_argument(std::string(function) + ": null callback");
}

std::uint32_t Stride(const JxrPixelInfo& info, std::uint32_t width) {
    std::uint32_t stride = 0;
    if (info.bitDepth == 0) {
        stride = (info.bitsPerUnit * width + 7) >> 3;
    } else {
        stride = ((info.bitsPerUnit + 7) >> 3) * width;
    }
    if (JxrIsTranscodeYuvStrideGuid_nid_no_patch(info.guid)) stride >>= 1;
    return stride;
}

std::uint32_t PixelStride(const JxrGuestGuid& guid, std::uint32_t width, const char* function) {
    JxrPixelInfo info{};
    info.guid = &guid;
    const auto result = PixelFormatLookup_nid_postfix(&info, 0);
    if (result < 0) NotImplemented_nid_no_patch(function);
    return Stride(info, width);
}

struct AlignedBuffer {
    void* value = nullptr;
    ~AlignedBuffer() {
        if (value != nullptr) PKFreeAligned_nid_postfix(&value);
    }
};

}

extern "C" std::int64_t PKImageEncode_Transcode_impl_nid_no_patch(void* encoder, void* converter, void* rectangle) {

    JxrGuestGuid fromFormat = JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch;
    JxrGuestGuid toFormat = JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch;

    const auto converterBeforeFrom = ConverterFields(converter, __func__);
    ValidateCallback(converterBeforeFrom.getSourcePixelFormat, __func__);
    const auto getSource = reinterpret_cast<GetPixelFormat>(converterBeforeFrom.getSourcePixelFormat);
    auto result = getSource(converter, &fromFormat);
    if (result < 0) return result;

    const auto converterBeforeTo = ConverterFields(converter, __func__);
    ValidateCallback(converterBeforeTo.getPixelFormat, __func__);
    const auto getTarget = reinterpret_cast<GetPixelFormat>(converterBeforeTo.getPixelFormat);
    result = getTarget(converter, &toFormat);
    if (result < 0) return result;

    const auto encoderBeforeCompare = EncoderFields(encoder, __func__);
    if (std::memcmp(encoderBeforeCompare.pixelFormat, &toFormat, sizeof(toFormat)) != 0) return UnsupportedFormat;

    const auto pixelStrideFrom = PixelStride(fromFormat, [&] {
        Rect rect{};
        CopyFromGuest(rect, rectangle, __func__);
        return rect.width;
    }(), __func__);
    const auto encoderBeforeStride = EncoderFields(encoder, __func__);
    const auto pixelStrideTo = PixelStride(toFormat, encoderBeforeStride.width, __func__);
    const auto stride = std::max(pixelStrideFrom, pixelStrideTo);

    const auto encoderBeforeBranch = EncoderFields(encoder, __func__);
    if (encoderBeforeBranch.isWmp) {
        const auto converterBeforeDecoder = ConverterFields(converter, __func__);
        if (converterBeforeDecoder.decoder == 0) throw std::invalid_argument("PKImageEncode_Transcode: null decoder");
        const auto decoder = reinterpret_cast<void*>(converterBeforeDecoder.decoder);
        const auto source = DecoderFields(decoder, __func__);
        const auto encoderBeforeCall = EncoderFields(encoder, __func__);
        ValidateCallback(encoderBeforeCall.transcode, __func__);
        TranscodingParam parameters{};
        parameters.leftX = source.roiLeft;
        parameters.topY = source.roiTop;
        parameters.width = source.roiWidth;
        parameters.height = source.roiHeight;
        parameters.orientation = source.orientation;
        parameters.alphaMode = source.alphaMode;
        parameters.bitstreamFormat = source.bitstreamFormat;
        parameters.subband = source.subband;
        parameters.ignoreOverlap = source.ignoreOverlap;
        const auto transcode = reinterpret_cast<Transcode>(encoderBeforeCall.transcode);
        return transcode(encoder, decoder, &parameters);
    }

    Rect rect{};
    CopyFromGuest(rect, rectangle, __func__);
    const auto size = static_cast<std::uint32_t>(stride * static_cast<std::uint32_t>(rect.height));

    AlignedBuffer buffer;
    result = PKAllocAligned_nid_postfix(&buffer.value, static_cast<std::size_t>(size), 128);
    if (result < 0) return result;
    if (buffer.value == nullptr) throw std::invalid_argument("PKImageEncode_Transcode: allocator returned null without an error");

    const auto converterBeforeCopy = ConverterFields(converter, __func__);
    ValidateCallback(converterBeforeCopy.copy, __func__);
    const auto copy = reinterpret_cast<CopyPixels>(converterBeforeCopy.copy);
    result = copy(converter, rectangle, static_cast<std::uint8_t*>(buffer.value), stride);
    if (result < 0) return result;

    CopyFromGuest(rect, rectangle, __func__);
    const auto encoderBeforeWrite = EncoderFields(encoder, __func__);
    ValidateCallback(encoderBeforeWrite.writePixels, __func__);
    const auto write = reinterpret_cast<WritePixels>(encoderBeforeWrite.writePixels);
    return write(encoder, static_cast<std::uint32_t>(rect.height), static_cast<std::uint8_t*>(buffer.value), stride);
}
