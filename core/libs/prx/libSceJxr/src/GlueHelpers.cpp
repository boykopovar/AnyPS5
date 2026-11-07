#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceJxr/include/GuidExports.hpp"
#include "ConverterCallbacks.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
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
std::int64_t JxrFormatConverterInitialize_nid_no_patch(void* converter, void* decoder, const void* sourceFormat,
                                                        const char* extension, const void* targetFormat,
                                                        std::int32_t* callbackId);
std::int64_t JxrConvertNative_nid_no_patch(void* converter, const void* rectangle, std::uint8_t* pixels,
                                           std::uint32_t stride, std::int32_t callbackId);

void* JxrGuestCalloc_nid_no_patch(std::size_t count, std::size_t size) {
    return calloc_nid_postfix(count, size);
}

void JxrGuestFree_nid_no_patch(void* memory) {
    free_nid_postfix(memory);
}
}

namespace {

constexpr std::uint32_t ImageUnsupported = 100;

#define JXR_DEFINE_CONVERTER_TRAMPOLINE(name) \
    std::int64_t APS5_VABI Convert_##name(JxrGuestFormatConverter* converter, const void* rectangle, \
                                           std::uint8_t* pixels, std::uint32_t stride) { \
        if (!rectangle) throw std::invalid_argument("PKFormatConverter Convert requires a rectangle"); \
        if constexpr (JXR_CONVERT_##name == JXR_CONVERT_BlackWhite_Gray8) { \
            if (!converter || !converter->decoder) \
                throw std::invalid_argument("PKFormatConverter BlackWhite conversion requires a decoder"); \
        } \
        return JxrConvertNative_nid_no_patch(converter, rectangle, pixels, stride, JXR_CONVERT_##name); \
    }
JXR_CONVERTER_CALLBACKS(JXR_DEFINE_CONVERTER_TRAMPOLINE)
#undef JXR_DEFINE_CONVERTER_TRAMPOLINE

void SetConverterCallback(JxrGuestFormatConverter* converter, JxrGuestFormatConvert callback) {
    auto* bytes = static_cast<std::byte*>(static_cast<void*>(converter));
    std::memcpy(bytes + 7 * sizeof(void*), &callback, sizeof(callback));
}

std::int64_t PKFormatConverterInitialize_nid_no_patch(JxrGuestFormatConverter* converter, void* decoder,
                                                       JxrGuestGuid sourceFormat, char* extension,
                                                       JxrGuestGuid targetFormat) {
    std::int32_t callbackId = -1;
    const auto result = JxrFormatConverterInitialize_nid_no_patch(converter, decoder, &sourceFormat, extension,
                                                                   &targetFormat, &callbackId);
    if (callbackId == -2)
        throw std::logic_error("PKFormatConverter_Initialize selected an unmapped converter");
    if (callbackId >= 0) {
        switch (callbackId) {
#define JXR_ASSIGN_CONVERTER_TRAMPOLINE(name) case JXR_CONVERT_##name: SetConverterCallback(converter, Convert_##name); break;
            JXR_CONVERTER_CALLBACKS(JXR_ASSIGN_CONVERTER_TRAMPOLINE)
#undef JXR_ASSIGN_CONVERTER_TRAMPOLINE
        default:
            throw std::logic_error("PKFormatConverter_Initialize selected an invalid converter");
        }
    }
    return result;
}

std::int64_t InitializeConverter(JxrGuestFormatConverter* converter, void* decoder, char* extension,
                                 JxrGuestGuid targetFormat) {
    if (!decoder) throw std::invalid_argument("PKFormatConverter_Initialize requires a decoder");
    using GetPixelFormat = std::int64_t(APS5_VABI*)(void*, JxrGuestGuid*);
    constexpr std::uintptr_t getPixelFormatOffset = sizeof(void*);
    const auto address = reinterpret_cast<std::uintptr_t>(decoder);
    if (address > std::numeric_limits<std::uintptr_t>::max() - getPixelFormatOffset)
        throw std::invalid_argument("PKFormatConverter_Initialize decoder callback address overflows");
    GetPixelFormat getPixelFormat = nullptr;
    if (!GuestCopyIn_nid_no_patch(&getPixelFormat, reinterpret_cast<const void*>(address + getPixelFormatOffset),
                                  sizeof(getPixelFormat)) || !getPixelFormat)
        throw std::invalid_argument("PKFormatConverter_Initialize requires a readable GetPixelFormat callback");

    JxrGuestGuid sourceFormat{};
    const auto formatResult = getPixelFormat(decoder, &sourceFormat);
    if (formatResult < 0) return formatResult;
    if (!converter) throw std::invalid_argument("PKFormatConverter_Initialize requires a converter");
    return PKFormatConverterInitialize_nid_no_patch(converter, decoder, sourceFormat, extension, targetFormat);
}

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

std::int64_t APS5_VABI PKFormatConverter_Initialize_nid_postfix(JxrGuestFormatConverter* converter, void* decoder,
                                                                 char* extension, JxrGuestGuid targetFormat) {
    return InitializeConverter(converter, decoder, extension, targetFormat);
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

}
