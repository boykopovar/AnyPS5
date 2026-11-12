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

#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/include/Api.hpp"
#include "src/EncoderMetadata.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include "Stream.hpp"
#include "Factory.hpp"
#include "prx/libSceJxr/include/GuidExports.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr std::int64_t WmpErrSuccess = 0;
constexpr std::int64_t WmpErrOutOfMemory = -101;
constexpr std::int64_t WmpErrUnsupportedFormat = -106;
constexpr std::int64_t WmpErrBufferOverflow = -103;
constexpr std::size_t MemoryBuffer = 0;
constexpr std::size_t MemorySize = 1;
constexpr std::size_t MemoryPosition = 2;

template<typename TSlot, typename TFunction>
TSlot Slot(TFunction* function) {
    return reinterpret_cast<TSlot>(function);
}

std::int64_t AllocateZeroed(void** out, std::size_t size) {
    *out = calloc_nid_postfix(1, size);
    return *out ? WmpErrSuccess : WmpErrOutOfMemory;
}

std::int64_t APS5_VABI CreateStreamFromFilename_nid_no_patch(Jxr::Stream**, const char*, const char*) {
    NotImplemented_nid_no_patch(__func__);
}

std::int64_t APS5_VABI CloseWS_Memory_nid_no_patch(Jxr::Stream** stream) {
    return WMPFree_nid_postfix(reinterpret_cast<void**>(stream));
}
}

extern "C" {

APS5_DUMMY_FUN
std::int64_t APS5_VABI PKFree_nid_postfix(void** memory) {
    if (memory != nullptr) {
        free_nid_postfix(*memory);

        *memory = nullptr;
    }
    return WmpErrSuccess;
}

std::int64_t APS5_VABI readIS_nid_postfix(void*, Jxr::BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    constexpr std::uintptr_t packetSize = 4096;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    if (((start ^ reinterpret_cast<std::uintptr_t>(io->current)) & packetSize) == 0) return WmpErrSuccess;
    auto* stream = io->stream;
    if (!stream || !stream->setPosition) APS5_INVALID_ARG_EX;
    stream->setPosition(stream, io->referenceOffset);
    if (!stream->read) APS5_INVALID_ARG_EX;
    {
        GuestArena::HostWrite access(io->start, packetSize);
        if (!access.Open()) APS5_INVALID_ARG_EX;
        stream->read(stream, io->start, packetSize);
    }
    io->referenceOffset += packetSize;
    if (!io->start) APS5_INVALID_ARG_EX;
    std::memcpy(&io->shadow, io->start, sizeof(io->shadow));
    const auto mask = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(io->mask));
    io->start = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(io->start) + packetSize) & mask);
    return WmpErrSuccess;
}

std::int64_t APS5_VABI writeIS_nid_postfix(void*, Jxr::BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    constexpr std::uintptr_t packetSize = 4096;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    if (((start ^ reinterpret_cast<std::uintptr_t>(io->current)) & packetSize) == 0) return WmpErrSuccess;
    if (!io->stream || !io->stream->write) APS5_INVALID_ARG_EX;
    const auto result = io->stream->write(io->stream, io->start, packetSize);
    if (result < 0) return result;
    if (!io->start) APS5_INVALID_ARG_EX;
    const auto mask = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(io->mask));
    io->start = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(io->start) + packetSize) & mask);
    return result;
}

int APS5_VABI AllocateCodingContextDec_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

std::int64_t APS5_VABI CreateWS_Memory_nid_postfix(Jxr::Stream** stream, void* buffer, std::size_t size) {
    if (!stream) APS5_INVALID_ARG_EX;
    const auto result = WMPAlloc_nid_postfix(reinterpret_cast<void**>(stream), sizeof(Jxr::Stream));
    if (result < 0) return result;
    auto* created = *stream;
    created->state[MemoryBuffer] = reinterpret_cast<std::uintptr_t>(buffer);
    created->state[MemorySize] = size;
    created->state[MemoryPosition] = 0;
    created->close = CloseWS_Memory_nid_no_patch;
    created->end = EOSWS_Memory_nid_postfix;
    created->read = ReadWS_Memory_nid_postfix;
    created->write = WriteWS_Memory_nid_postfix;
    created->setPosition = SetPosWS_Memory_nid_postfix;
    created->getPosition = GetPosWS_Memory_nid_postfix;
    return WmpErrSuccess;
}

std::int32_t APS5_VABI EOSWS_Memory_nid_postfix(Jxr::Stream* stream) {
    if (!stream) APS5_INVALID_ARG_EX;
    return stream->state[MemorySize] <= stream->state[MemoryPosition];
}

int APS5_VABI FreeDescMetadata_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

std::int64_t APS5_VABI GetPosWS_Memory_nid_postfix(Jxr::Stream* stream, std::size_t* position) {
    if (!stream || !position) APS5_INVALID_ARG_EX;
    *position = stream->state[MemoryPosition];
    return WmpErrSuccess;
}

std::int64_t APS5_VABI PKCodecFactory_CreateCodec_nid_postfix(const std::uint32_t* iid, void** codec) {
    if (!iid || !codec) APS5_INVALID_ARG_EX;
    if (*iid == JxrGuest_IID_PKImageWmpEncode_nid_no_patch) {
        return reinterpret_cast<std::int64_t (APS5_VABI*)(void**)>(&PKImageEncode_Create_WMP_nid_postfix)(codec);
    }
    if (*iid == JxrGuest_IID_PKImageWmpDecode_nid_no_patch) {
        return reinterpret_cast<std::int64_t (APS5_VABI*)(void**)>(&PKImageDecode_Create_WMP_nid_postfix)(codec);
    }
    return WmpErrUnsupportedFormat;
}

int APS5_VABI PKCodecFactory_CreateDecoderFromFile_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

std::int64_t APS5_VABI PKCodecFactory_CreateFormatConverter_nid_postfix(void** converter) {
    if (!converter) APS5_INVALID_ARG_EX;
    const auto result = AllocateZeroed(converter, sizeof(Jxr::FormatConverter));
    if (result != WmpErrSuccess) return result;
    auto* value = static_cast<Jxr::FormatConverter*>(*converter);
    value->initialize = Slot<decltype(value->initialize)>(&PKFormatConverter_Initialize_nid_postfix);
    value->initializeConvert = Slot<decltype(value->initializeConvert)>(&PKFormatConverter_InitializeConvert_nid_postfix);
    value->getPixelFormat = Slot<decltype(value->getPixelFormat)>(&PKFormatConverter_GetPixelFormat_nid_postfix);
    value->getSourcePixelFormat = Slot<decltype(value->getSourcePixelFormat)>(&PKFormatConverter_GetSourcePixelFormat_nid_postfix);
    value->getSize = Slot<decltype(value->getSize)>(&PKFormatConverter_GetSize_nid_postfix);
    value->getResolution = Slot<decltype(value->getResolution)>(&PKFormatConverter_GetResolution_nid_postfix);
    value->copy = Slot<decltype(value->copy)>(&PKFormatConverter_Copy_nid_postfix);
    value->convert = Slot<decltype(value->convert)>(&PKFormatConverter_Convert_nid_postfix);
    value->release = Slot<decltype(value->release)>(&PKFormatConverter_Release_nid_postfix);
    return WmpErrSuccess;
}

std::int64_t APS5_VABI PKCreateCodecFactory_Release_nid_postfix(void** factory) {
    return PKFree_nid_postfix(factory);
}

std::int64_t APS5_VABI PKCreateCodecFactory_nid_postfix(void** factory, std::uint32_t) {
    if (!factory) APS5_INVALID_ARG_EX;
    const auto result = AllocateZeroed(factory, sizeof(Jxr::CodecFactory));
    if (result != WmpErrSuccess) return result;
    auto* value = static_cast<Jxr::CodecFactory*>(*factory);
    value->createCodec = &PKCodecFactory_CreateCodec_nid_postfix;
    value->createDecoderFromFile = Slot<decltype(value->createDecoderFromFile)>(&PKCodecFactory_CreateDecoderFromFile_nid_postfix);
    value->createFormatConverter = &PKCodecFactory_CreateFormatConverter_nid_postfix;
    value->release = Slot<decltype(value->release)>(&PKCreateCodecFactory_Release_nid_postfix);
    return WmpErrSuccess;
}

std::int64_t APS5_VABI PKCreateFactory_CreateStream_nid_postfix(void** stream) {
    if (!stream) APS5_INVALID_ARG_EX;
    return AllocateZeroed(stream, sizeof(Jxr::PlainStream));
}

std::int64_t APS5_VABI PKCreateFactory_Release_nid_postfix(void** factory) {
    return PKFree_nid_postfix(factory);
}

std::int64_t APS5_VABI PKCreateFactory_nid_postfix(void** factory, std::uint32_t) {
    if (!factory) APS5_INVALID_ARG_EX;
    const auto result = AllocateZeroed(factory, sizeof(Jxr::Factory));
    if (result != WmpErrSuccess) return result;
    auto* value = static_cast<Jxr::Factory*>(*factory);
    value->createStream = Slot<decltype(value->createStream)>(&PKCreateFactory_CreateStream_nid_postfix);
    value->createStreamFromFilename = &CreateStreamFromFilename_nid_no_patch;
    value->createStreamFromMemory = Slot<decltype(value->createStreamFromMemory)>(&CreateWS_Memory_nid_postfix);
    value->release = Slot<decltype(value->release)>(&PKCreateFactory_Release_nid_postfix);
    return WmpErrSuccess;
}

std::int64_t APS5_VABI PKFormatConverter_Convert_nid_postfix(void* converter, const void* rect, std::uint8_t* pixels, std::uint32_t stride) {
    return WmpErrSuccess;
}

int APS5_VABI PKFormatConverter_Copy_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_EnumConversions_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_GetPixelFormat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_GetResolution_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_GetSize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_GetSourcePixelFormat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_InitializeConvert_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_Initialize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Copy_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Create_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Create_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

std::int64_t APS5_VABI PKImageDecode_SelectFrame_nid_postfix(void* decoder, std::uint32_t frame) {
    return WmpErrSuccess;
}

std::int64_t APS5_VABI PKImageEncode_CreateNewFrame_nid_postfix(void* encoder, void* param, std::size_t paramSize) {
    return WmpErrSuccess;
}

int APS5_VABI PKImageEncode_Create_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

std::int64_t APS5_VABI PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(
    tagPKImageEncode* encoder, const std::uint8_t* metadata, std::uint32_t size) {
    return Jxr::SetImageEncodeMetadata(
        static_cast<void*>(encoder), metadata, size, offsetof(Jxr::ImageEncodeMetadataPrefix, exifMetadata),
        offsetof(Jxr::ImageEncodeMetadataPrefix, exifMetadataByteCount));
}

std::int64_t APS5_VABI PKImageEncode_SetGPSInfoMetadata_WMP_nid_postfix(
    tagPKImageEncode* encoder, const std::uint8_t* metadata, std::uint32_t size) {
    return Jxr::SetImageEncodeMetadata(
        static_cast<void*>(encoder), metadata, size, offsetof(Jxr::ImageEncodeMetadataPrefix, gpsInfoMetadata),
        offsetof(Jxr::ImageEncodeMetadataPrefix, gpsInfoMetadataByteCount));
}

std::int64_t APS5_VABI PKImageEncode_SetIPTCNAAMetadata_WMP_nid_postfix(
    tagPKImageEncode* encoder, const std::uint8_t* metadata, std::uint32_t size) {
    return Jxr::SetImageEncodeMetadata(
        static_cast<void*>(encoder), metadata, size, offsetof(Jxr::ImageEncodeMetadataPrefix, iptcNaaMetadata),
        offsetof(Jxr::ImageEncodeMetadataPrefix, iptcNaaMetadataByteCount));
}

std::int64_t APS5_VABI PKImageEncode_SetPhotoshopMetadata_WMP_nid_postfix(
    tagPKImageEncode* encoder, const std::uint8_t* metadata, std::uint32_t size) {
    return Jxr::SetImageEncodeMetadata(
        static_cast<void*>(encoder), metadata, size, offsetof(Jxr::ImageEncodeMetadataPrefix, photoshopMetadata),
        offsetof(Jxr::ImageEncodeMetadataPrefix, photoshopMetadataByteCount));
}

std::int64_t APS5_VABI PKImageEncode_SetXMPMetadata_WMP_nid_postfix(tagPKImageEncode* encoder,
                                                                   const std::uint8_t* metadata, std::uint32_t size) {
    if (!encoder) throw std::invalid_argument("XMP metadata setter requires an encoder");
    auto* prefix = reinterpret_cast<Jxr::ImageEncodeMetadataPrefix*>(encoder);
    if (prefix->headerDone != 0) {
        prefix->xmpMetadataByteCount = 0;
        return Jxr::WmpErrOutOfSequence;
    }

    auto* record = reinterpret_cast<std::byte*>(encoder);
    auto** stored = reinterpret_cast<std::uint8_t**>(record + offsetof(Jxr::ImageEncodeMetadataPrefix, xmpMetadata));
    auto* storedSize = reinterpret_cast<std::uint32_t*>(record + offsetof(Jxr::ImageEncodeMetadataPrefix, xmpMetadataByteCount));
    void* previous = *stored;
    PKFree_nid_postfix(&previous);
    *stored = static_cast<std::uint8_t*>(previous);
    *storedSize = 0;

    constexpr char format[] = "<dc:format>image/vnd.ms-photo</dc:format>";
    constexpr char opening[] = "<dc:format>";
    constexpr char closing[] = "</dc:format>";
    constexpr std::size_t extra = 1 + sizeof(opening) - 1 + sizeof(closing) - 1 + sizeof(format) - 1;
    const auto allocationSize = static_cast<std::size_t>(static_cast<std::uint32_t>(size + 1U)) + extra - 1;
    void* allocation = nullptr;
    const auto allocationResult = PKAlloc_nid_postfix(&allocation, allocationSize);
    if (allocationResult < 0) return allocationResult;
    if (size == UINT32_MAX) {
        PKFree_nid_postfix(&allocation);
        throw std::length_error("XMP metadata size overflows the source length field");
    }
    if (size != 0 && metadata == nullptr) {
        PKFree_nid_postfix(&allocation);
        throw std::invalid_argument("XMP metadata setter requires input bytes");
    }
    auto* temporary = static_cast<char*>(allocation);
    if (size != 0) std::memcpy(temporary, metadata, size);
    temporary[size] = '\0';
    const auto inputSize = std::strlen(temporary);
    auto* formatBegin = std::strstr(temporary, opening);
    std::size_t outputSize = inputSize;
    if (formatBegin != nullptr) {
        auto* formatEnd = std::strstr(formatBegin, closing);
        auto* nestedTag = std::strchr(formatBegin + sizeof(opening) - 1, '<');
        if (formatEnd == nullptr || nestedTag != formatEnd) {
            PKFree_nid_postfix(&allocation);
            return -1;
        }
        formatEnd += sizeof(closing) - 1;
        const auto beginOffset = static_cast<std::size_t>(formatBegin - temporary);
        const auto endOffset = static_cast<std::size_t>(formatEnd - temporary);
        outputSize = inputSize - (endOffset - beginOffset) + sizeof(format) - 1;
        const auto tailSize = inputSize - endOffset;
        const auto* originalTail = metadata + endOffset;
        std::memcpy(formatBegin, format, sizeof(format));
        std::memcpy(formatBegin + sizeof(format) - 1, originalTail, tailSize);
    }
    *stored = static_cast<std::uint8_t*>(allocation);
    *storedSize = static_cast<std::uint32_t>(outputSize);
    return allocationResult;
}

std::int64_t APS5_VABI PKImageEncode_Terminate_nid_postfix(void* encoder) {
    return WmpErrSuccess;
}

int APS5_VABI PKImageEncode_Transcode_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_WriteSource_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

std::int64_t APS5_VABI ReadWS_Memory_nid_postfix(Jxr::Stream* stream, void* buffer, std::size_t size) {
    if (!stream) APS5_INVALID_ARG_EX;
    auto& state = stream->state;
    if (state[MemorySize] < state[MemoryPosition]) return WmpErrSuccess;
    if (state[MemoryPosition] + size < state[MemoryPosition]) return WmpErrBufferOverflow;
    if (state[MemorySize] < state[MemoryPosition] + size) size = state[MemorySize] - state[MemoryPosition];
    if (size != 0) {
        if (!buffer || !state[MemoryBuffer]) APS5_INVALID_ARG_EX;
        std::memcpy(buffer, reinterpret_cast<const std::uint8_t*>(state[MemoryBuffer]) + state[MemoryPosition], size);
    }
    state[MemoryPosition] += size;
    return WmpErrSuccess;
}

std::int64_t APS5_VABI SetPosWS_Memory_nid_postfix(Jxr::Stream* stream, std::size_t position) {
    if (!stream) APS5_INVALID_ARG_EX;
    stream->state[MemoryPosition] = position;
    return WmpErrSuccess;
}

std::int64_t APS5_VABI WMPAlloc_nid_postfix(void** memory, std::size_t size) {
    if (!memory) APS5_INVALID_ARG_EX;
    *memory = calloc_nid_postfix(1, size);
    return *memory ? WmpErrSuccess : WmpErrOutOfMemory;
}

std::int64_t APS5_VABI WMPFree_nid_postfix(void** memory) {
    if (!memory) APS5_INVALID_ARG_EX;
    if (*memory) {
        free_nid_postfix(*memory);
        *memory = nullptr;
    }
    return WmpErrSuccess;
}

std::int64_t APS5_VABI WriteWS_Memory_nid_postfix(Jxr::Stream* stream, const void* buffer, std::size_t size) {
    if (!stream) APS5_INVALID_ARG_EX;
    auto& state = stream->state;
    if (state[MemoryPosition] + size < state[MemoryPosition] || state[MemorySize] < state[MemoryPosition] + size) return WmpErrBufferOverflow;
    if (size != 0) {
        if (!buffer || !state[MemoryBuffer]) APS5_INVALID_ARG_EX;
        std::memcpy(reinterpret_cast<std::uint8_t*>(state[MemoryBuffer]) + state[MemoryPosition], buffer, size);
    }
    state[MemoryPosition] += size;
    return WmpErrSuccess;
}

std::int64_t APS5_VABI detachISRead_nid_postfix(void* codec, Jxr::BitIo* io) {
    if (!io || !io->stream) APS5_INVALID_ARG_EX;
    auto* stream = io->stream;
    flushToByte_nid_postfix(io);
    if ((io->bitsUsed & 7) != 0) APS5_INVALID_ARG_EX;
    const auto result = readIS_nid_postfix(codec, io);
    if (result < 0) return result;
    constexpr std::uintptr_t packetSize = 8192;
    if (!io->start || !io->current) APS5_INVALID_ARG_EX;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    const auto current = reinterpret_cast<std::uintptr_t>(io->current);
    const auto byteOffset = static_cast<std::uintptr_t>(io->bitsUsed / 8);
    if (start > UINTPTR_MAX - packetSize || current > UINTPTR_MAX - byteOffset) APS5_INVALID_ARG_EX;
    const auto end = start + packetSize;
    const auto position = current + byteOffset;
    const auto remaining = end - position;
    if (!stream->setPosition) APS5_INVALID_ARG_EX;
    stream->setPosition(stream, io->referenceOffset - remaining);
    io->stream = nullptr;
    return result;
}

std::int64_t APS5_VABI detachISWrite_nid_postfix(void* codec, Jxr::BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    if ((io->bitsUsed & 7) != 0) APS5_INVALID_ARG_EX;
    const auto result = writeIS_nid_postfix(codec, io);
    if (result < 0) return result;
    if (!io->stream || !io->stream->write || !io->start || !io->current) APS5_INVALID_ARG_EX;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    const auto current = reinterpret_cast<std::uintptr_t>(io->current);
    const auto byteCount = static_cast<std::uintptr_t>(io->bitsUsed / 8);
    if (current > UINTPTR_MAX - byteCount) APS5_INVALID_ARG_EX;
    const auto end = current + byteCount;
    const auto writeResult = io->stream->write(io->stream, io->start, end - start);
    if (writeResult < 0) return writeResult;
    io->stream = nullptr;
    return writeResult;
}

std::int32_t APS5_VABI initPostProc_nid_postfix(JxrGuestPostProcInfo* (*postProcInfo)[2], std::size_t mbWidth, std::size_t numChannels) {
    if (numChannels != 0 && postProcInfo == nullptr) APS5_INVALID_ARG_EX;

    for (std::size_t channel = 0; channel < numChannels; ++channel) {
        for (std::size_t slot = 0; slot < 2; ++slot) {
            if (sizeof(int) == 4 && ((((mbWidth + 2) >> 16) * sizeof(JxrGuestPostProcInfo)) & 0xffff0000)) return -1;

            if (mbWidth > std::numeric_limits<std::size_t>::max() - 2 ||
                mbWidth + 2 > std::numeric_limits<std::size_t>::max() / sizeof(JxrGuestPostProcInfo)) APS5_INVALID_ARG_EX;

            auto* allocation = static_cast<JxrGuestPostProcInfo*>(malloc_nid_postfix((mbWidth + 2) * sizeof(JxrGuestPostProcInfo)));
            postProcInfo[channel][slot] = allocation;
            if (allocation == nullptr) return -1;
            ++postProcInfo[channel][slot];

            auto* leftBoundary = postProcInfo[channel][slot] - 1;
            leftBoundary->mbTexture = 3;
            for (std::size_t row = 0; row < 4; ++row) {
                for (std::size_t column = 0; column < 4; ++column) {
                    leftBoundary->blockTexture[row][column] = 3;
                }
            }
            std::memcpy(postProcInfo[channel][slot] + mbWidth, leftBoundary, sizeof(*leftBoundary));
        }
    }

    return 0;
}

void APS5_VABI termPostProc_nid_postfix(JxrGuestPostProcInfo* (*postProcInfo)[2], std::size_t numChannels) {
    if (numChannels != 0 && postProcInfo == nullptr) APS5_INVALID_ARG_EX;

    for (std::size_t channel = 0; channel < numChannels; ++channel) {
        for (std::size_t slot = 0; slot < 2; ++slot) {
            if (postProcInfo[channel][slot] != nullptr) {
                free_nid_postfix(postProcInfo[channel][slot] - 1);
            }
        }
    }
}

}
