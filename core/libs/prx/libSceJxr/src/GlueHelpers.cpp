#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include "DecodeCallbacks.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

extern "C" {
const void* JxrHashLookup_nid_no_patch(std::uint8_t hash);
std::int64_t JxrEncodeIid_nid_no_patch(const char* extension, std::uint32_t* value);
std::int64_t JxrDecodeIid_nid_no_patch(const char* extension, std::uint32_t* value);
std::int64_t JxrAllocAligned_nid_no_patch(void** memory, std::size_t size, std::size_t alignment);
std::int64_t JxrFreeAligned_nid_no_patch(void** memory);
std::int32_t JxrIdpEmpty_nid_no_patch(void* codec);
std::int64_t JxrDecodeCreateGuest_nid_no_patch(void** decoder, const JxrDecodeCallbacks* callbacks);
std::int32_t JxrDecodeHasGetPosGuest_nid_no_patch(void* stream);
std::int32_t JxrDecodeHasCloseGuest_nid_no_patch(void* decoder);
std::int64_t JxrDecodeInitializeNative_nid_no_patch(void* decoder, void* stream);
std::int64_t JxrDecodeGetPixelFormatNative_nid_no_patch(void* decoder, void* format);
std::int64_t JxrDecodeGetSizeNative_nid_no_patch(void* decoder, std::int32_t* width, std::int32_t* height);
std::int64_t JxrDecodeGetResolutionNative_nid_no_patch(void* decoder, float* x, float* y);
std::int64_t JxrDecodeGetColorContextNative_nid_no_patch(void* decoder, std::uint8_t* buffer, std::uint32_t* size);
std::int64_t JxrDecodeGetDescriptiveMetadataNative_nid_no_patch(void* decoder, void* metadata);
std::int64_t JxrDecodeCopyNative_nid_no_patch(void* decoder, const void* rectangle, std::uint8_t* buffer, std::uint32_t stride);
std::int64_t JxrDecodeGetFrameCountNative_nid_no_patch(void* decoder, std::uint32_t* count);
std::int64_t JxrDecodeSelectFrameNative_nid_no_patch(void* decoder, std::uint32_t frame);
std::int64_t JxrDecodeReleaseNative_nid_no_patch(void** decoder);

void* JxrGuestCalloc_nid_no_patch(std::size_t count, std::size_t size) {
    return calloc_nid_postfix(count, size);
}

void JxrGuestFree_nid_no_patch(void* memory) {
    free_nid_postfix(memory);
}
}

namespace {

constexpr std::uint32_t ImageUnsupported = 100;

std::int64_t StoreIid(std::int64_t (*lookup)(const char*, std::uint32_t*), const char* extension,
                      const std::uint32_t** iid, const char* function) {
    if (!extension) throw std::invalid_argument(std::string(function) + " requires an extension");
    std::uint32_t value = 0;
    const auto result = lookup(extension, &value);
    if (result < 0) return result;
    if (!iid) throw std::invalid_argument(std::string(function) + " requires an output for a known extension");
    switch (value) {
    case 101: *iid = &JxrGuest_IID_PKImageWmpEncode_nid_no_patch; break;
    case 201: *iid = &JxrGuest_IID_PKImageWmpDecode_nid_no_patch; break;
    case ImageUnsupported: *iid = &ImageUnsupported; break;
    default: throw std::logic_error(std::string(function) + ": jxrlib returned an unknown IID " + std::to_string(value) + " for " + extension);
    }
    return result;
}

std::int64_t APS5_VABI DecodeInitializeGuest(void* decoder, void* stream) {
    if (!decoder || !stream) throw std::invalid_argument("PKImageDecode Initialize requires a decoder and stream");
    if (!JxrDecodeHasGetPosGuest_nid_no_patch(stream)) throw std::invalid_argument("PKImageDecode Initialize requires GetPos");
    return JxrDecodeInitializeNative_nid_no_patch(decoder, stream);
}

std::int64_t APS5_VABI DecodeGetPixelFormatGuest(void* decoder, void* format) {
    if (!decoder || !format) throw std::invalid_argument("PKImageDecode GetPixelFormat requires a decoder and output");
    return JxrDecodeGetPixelFormatNative_nid_no_patch(decoder, format);
}

std::int64_t APS5_VABI DecodeGetSizeGuest(void* decoder, std::int32_t* width, std::int32_t* height) {
    if (!decoder || !width || !height) throw std::invalid_argument("PKImageDecode GetSize requires a decoder and outputs");
    return JxrDecodeGetSizeNative_nid_no_patch(decoder, width, height);
}

std::int64_t APS5_VABI DecodeGetResolutionGuest(void* decoder, float* x, float* y) {
    if (!decoder || !x || !y) throw std::invalid_argument("PKImageDecode GetResolution requires a decoder and outputs");
    return JxrDecodeGetResolutionNative_nid_no_patch(decoder, x, y);
}

std::int64_t APS5_VABI DecodeGetColorContextGuest(void* decoder, std::uint8_t* buffer, std::uint32_t* size) {
    return JxrDecodeGetColorContextNative_nid_no_patch(decoder, buffer, size);
}

std::int64_t APS5_VABI DecodeGetDescriptiveMetadataGuest(void* decoder, void* metadata) {
    return JxrDecodeGetDescriptiveMetadataNative_nid_no_patch(decoder, metadata);
}

std::int64_t APS5_VABI DecodeCopyGuest(void* decoder, const void* rectangle, std::uint8_t* buffer, std::uint32_t stride) {
    return JxrDecodeCopyNative_nid_no_patch(decoder, rectangle, buffer, stride);
}

std::int64_t APS5_VABI DecodeGetFrameCountGuest(void* decoder, std::uint32_t* count) {
    if (!decoder || !count) throw std::invalid_argument("PKImageDecode GetFrameCount requires a decoder and output");
    return JxrDecodeGetFrameCountNative_nid_no_patch(decoder, count);
}

std::int64_t APS5_VABI DecodeSelectFrameGuest(void* decoder, std::uint32_t frame) {
    return JxrDecodeSelectFrameNative_nid_no_patch(decoder, frame);
}

std::int64_t APS5_VABI DecodeReleaseGuest(void** decoder) {
    if (!decoder || !*decoder) throw std::invalid_argument("PKImageDecode Release requires a decoder");
    if (!JxrDecodeHasCloseGuest_nid_no_patch(*decoder)) throw std::invalid_argument("PKImageDecode Release requires a stream Close callback when it owns the stream");
    return JxrDecodeReleaseNative_nid_no_patch(decoder);
}

const JxrDecodeCallbacks DecodeCallbacks{
    DecodeInitializeGuest,
    DecodeGetPixelFormatGuest,
    DecodeGetSizeGuest,
    DecodeGetResolutionGuest,
    DecodeGetColorContextGuest,
    DecodeGetDescriptiveMetadataGuest,
    DecodeCopyGuest,
    DecodeGetFrameCountGuest,
    DecodeSelectFrameGuest,
    DecodeReleaseGuest};

} // namespace

extern "C" {

const void* APS5_VABI GetPixelFormatFromHash_nid_postfix(std::uint8_t hash) {
    return JxrHashLookup_nid_no_patch(hash);
}

std::int64_t APS5_VABI GetImageEncodeIID_nid_postfix(const char* extension, const std::uint32_t** iid) {
    return StoreIid(JxrEncodeIid_nid_no_patch, extension, iid, "GetImageEncodeIID");
}

std::int64_t APS5_VABI GetImageDecodeIID_nid_postfix(const char* extension, const std::uint32_t** iid) {
    return StoreIid(JxrDecodeIid_nid_no_patch, extension, iid, "GetImageDecodeIID");
}

std::int64_t APS5_VABI PKAllocAligned_nid_postfix(void** memory, std::size_t size, std::size_t alignment) {
    if (!memory) throw std::invalid_argument("PKAllocAligned requires an output pointer");
    *memory = nullptr;
    if (alignment < sizeof(void*))
        throw std::invalid_argument("PKAllocAligned: alignments below 8 can leave no room for the original pointer");
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    if (size > maximum - sizeof(void*) || alignment - 1 > maximum - sizeof(void*) - size)
        throw std::length_error("PKAllocAligned: block size overflows size_t");
    return JxrAllocAligned_nid_no_patch(memory, size, alignment);
}

std::int64_t APS5_VABI PKFreeAligned_nid_postfix(void** memory) {
    return JxrFreeAligned_nid_no_patch(memory);
}

std::int32_t APS5_VABI IDPEmpty_nid_postfix(void* codec) {
    return JxrIdpEmpty_nid_no_patch(codec);
}

std::int64_t APS5_VABI PKImageDecode_Create_nid_postfix(void** decoder) {
    if (!decoder) throw std::invalid_argument("PKImageDecode_Create requires an output");
    return JxrDecodeCreateGuest_nid_no_patch(decoder, &DecodeCallbacks);
}

}
