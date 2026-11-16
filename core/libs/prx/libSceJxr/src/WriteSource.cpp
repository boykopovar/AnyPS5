#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libSceJxr/include/GuidExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
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
using CopyPixels = Err(APS5_VABI*)(void*, void*, std::uint8_t*, std::uint32_t);
using WritePixels = Err(APS5_VABI*)(void*, std::uint32_t, std::uint8_t*, std::uint32_t);

constexpr Err UnsupportedFormat = -106;

extern "C" std::size_t JxrWriteSourceSourceCallbackSize_nid_no_patch();
extern "C" std::size_t JxrWriteSourceTargetCallbackSize_nid_no_patch();
extern "C" std::size_t JxrWriteSourceCopyCallbackSize_nid_no_patch();
extern "C" std::size_t JxrWriteSourceEncoderFormatSize_nid_no_patch();
extern "C" std::size_t JxrWriteSourceEncoderWidthSize_nid_no_patch();
extern "C" std::size_t JxrWriteSourceEncoderCallbackSize_nid_no_patch();
extern "C" std::size_t JxrWriteSourceRectWidthOffset_nid_no_patch();
extern "C" std::size_t JxrWriteSourceRectHeightOffset_nid_no_patch();
extern "C" std::uintptr_t JxrWriteSourceSourceCallback_nid_no_patch(const void* record);
extern "C" std::uintptr_t JxrWriteSourceTargetCallback_nid_no_patch(const void* record);
extern "C" std::uintptr_t JxrWriteSourceCopyCallback_nid_no_patch(const void* record);
extern "C" void JxrWriteSourceEncoderFormat_nid_no_patch(const void* record, void* format);
extern "C" std::uint32_t JxrWriteSourceEncoderWidth_nid_no_patch(const void* record);
extern "C" std::uintptr_t JxrWriteSourceEncoderCallback_nid_no_patch(const void* record);
extern "C" std::int32_t JxrWriteSourceYuvStrideGuid_nid_no_patch(const void* guid);

template <typename T>
void CopyFromGuest(T& value, const void* address, const char* function) {
    if (address == nullptr || !GuestCopyIn_nid_no_patch(&value, address, sizeof(value)))
        throw std::invalid_argument(std::string(function) + ": unreadable guest data");
}

std::vector<std::byte> ReadRecord(const void* address, std::size_t size, const char* function) {
    if (address == nullptr) throw std::invalid_argument(std::string(function) + ": null guest record");
    std::vector<std::byte> record(size);
    if (!GuestCopyIn_nid_no_patch(record.data(), address, record.size()))
        throw std::invalid_argument(std::string(function) + ": unreadable guest record");
    return record;
}

std::uintptr_t ConverterCallback(const void* converter, std::size_t size, std::uintptr_t (*get)(const void*),
                                 const char* function) {
    const auto record = ReadRecord(converter, size, function);
    return get(record.data());
}

void ValidateCallback(std::uintptr_t address, const char* function) {
    if (address == 0) throw std::invalid_argument(std::string(function) + ": null callback");
}

JxrPixelInfo LookupPixelInfo(const JxrGuestGuid& guid, const char* function) {
    JxrPixelInfo info{};
    info.guid = &guid;
    const auto result = PixelFormatLookup_nid_postfix(&info, 0);
    if (result < 0) throw std::runtime_error(std::string(function) + ": unsupported pixel format lookup");
    return info;
}

std::uint32_t Stride(const JxrPixelInfo& info, const JxrGuestGuid& guid, std::uint32_t width) {
    std::uint32_t stride;
    if (info.bitDepth == 0) {
        stride = (info.bitsPerUnit * width + 7) >> 3;
    } else {
        stride = ((info.bitsPerUnit + 7) >> 3) * width;
    }
    if (JxrWriteSourceYuvStrideGuid_nid_no_patch(&guid)) stride >>= 1;
    return stride;
}

std::int32_t ReadRectWidth(const void* rectangle, const char* function) {
    if (rectangle == nullptr) throw std::invalid_argument(std::string(function) + ": null rectangle");
    const auto address = reinterpret_cast<std::uintptr_t>(rectangle);
    const auto offset = JxrWriteSourceRectWidthOffset_nid_no_patch();
    if (address > std::numeric_limits<std::uintptr_t>::max() - offset)
        throw std::invalid_argument(std::string(function) + ": invalid rectangle address");
    std::int32_t width = 0;
    CopyFromGuest(width, reinterpret_cast<const void*>(address + offset), function);
    return width;
}

std::int32_t ReadRectHeight(const void* rectangle, const char* function) {
    if (rectangle == nullptr) throw std::invalid_argument(std::string(function) + ": null rectangle");
    const auto address = reinterpret_cast<std::uintptr_t>(rectangle);
    const auto offset = JxrWriteSourceRectHeightOffset_nid_no_patch();
    if (address > std::numeric_limits<std::uintptr_t>::max() - offset)
        throw std::invalid_argument(std::string(function) + ": invalid rectangle address");
    std::int32_t height = 0;
    CopyFromGuest(height, reinterpret_cast<const void*>(address + offset), function);
    return height;
}

struct AlignedBuffer {
    void* value = nullptr;
    ~AlignedBuffer() {
        if (value != nullptr) PKFreeAligned_nid_postfix(&value);
    }
};

}

extern "C" std::int64_t PKImageEncode_WriteSource_impl_nid_no_patch(void* encoder, void* converter, void* rectangle) {
    JxrGuestGuid fromFormat = JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch;
    JxrGuestGuid toFormat = JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch;

    const auto sourceAddress = ConverterCallback(converter, JxrWriteSourceSourceCallbackSize_nid_no_patch(),
                                                 JxrWriteSourceSourceCallback_nid_no_patch, __func__);
    ValidateCallback(sourceAddress, __func__);
    auto result = reinterpret_cast<GetPixelFormat>(sourceAddress)(converter, &fromFormat);
    if (result < 0) return result;

    const auto targetAddress = ConverterCallback(converter, JxrWriteSourceTargetCallbackSize_nid_no_patch(),
                                                 JxrWriteSourceTargetCallback_nid_no_patch, __func__);
    ValidateCallback(targetAddress, __func__);
    result = reinterpret_cast<GetPixelFormat>(targetAddress)(converter, &toFormat);
    if (result < 0) return result;

    const auto encoderFormatRecord = ReadRecord(encoder, JxrWriteSourceEncoderFormatSize_nid_no_patch(), __func__);
    JxrGuestGuid encoderFormat{};
    JxrWriteSourceEncoderFormat_nid_no_patch(encoderFormatRecord.data(), &encoderFormat);
    if (std::memcmp(&encoderFormat, &toFormat, sizeof(toFormat)) != 0) return UnsupportedFormat;

    const auto fromInfo = LookupPixelInfo(fromFormat, __func__);
    const auto toInfo = LookupPixelInfo(toFormat, __func__);
    const auto width = static_cast<std::uint32_t>(ReadRectWidth(rectangle, __func__));
    const auto strideFrom = Stride(fromInfo, fromFormat, width);
    const auto encoderWidthRecord = ReadRecord(encoder, JxrWriteSourceEncoderWidthSize_nid_no_patch(), __func__);
    const auto widthTo = JxrWriteSourceEncoderWidth_nid_no_patch(encoderWidthRecord.data());
    const auto strideTo = Stride(toInfo, toFormat, widthTo);
    const auto stride = std::max(strideFrom, strideTo);
    const auto height = static_cast<std::uint32_t>(ReadRectHeight(rectangle, __func__));
    const auto allocationSize = static_cast<std::uint32_t>(stride * height);

    AlignedBuffer buffer;
    result = PKAllocAligned_nid_postfix(&buffer.value, allocationSize, 128);
    if (result < 0) return result;
    if (buffer.value == nullptr) throw std::invalid_argument("PKImageEncode_WriteSource: allocator returned null");

    const auto copyAddress = ConverterCallback(converter, JxrWriteSourceCopyCallbackSize_nid_no_patch(),
                                               JxrWriteSourceCopyCallback_nid_no_patch, __func__);
    ValidateCallback(copyAddress, __func__);
    result = reinterpret_cast<CopyPixels>(copyAddress)(converter, rectangle,
                                                       static_cast<std::uint8_t*>(buffer.value), stride);
    if (result < 0) return result;

    const auto writeHeight = static_cast<std::uint32_t>(ReadRectHeight(rectangle, __func__));
    const auto encoderCallbackRecord = ReadRecord(encoder, JxrWriteSourceEncoderCallbackSize_nid_no_patch(), __func__);
    const auto writeAddress = JxrWriteSourceEncoderCallback_nid_no_patch(encoderCallbackRecord.data());
    ValidateCallback(writeAddress, __func__);
    return reinterpret_cast<WritePixels>(writeAddress)(encoder, writeHeight,
                                                       static_cast<std::uint8_t*>(buffer.value), stride);
}
