#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "Stream.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr std::int64_t WmpErrSuccess = 0;
constexpr std::uint32_t ShortBits = 16;
constexpr std::uint32_t LongBits = 32;

std::uint32_t LoadBigEndian(const std::uint8_t* bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) | static_cast<std::uint32_t>(bytes[3]);
}

std::uint32_t PeekBits(const Jxr::BitIo* io, std::uint32_t bits) {
    return io->accumulator >> (LongBits - bits);
}

void AdvanceBits(Jxr::BitIo* io, std::uint32_t bits, const char* name) {
    const std::uint32_t used = io->bitsUsed + bits;
    const auto mask = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(io->mask));
    auto* current = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(io->current) + (used >> 3)) & mask);
    if (!current) throw std::invalid_argument(std::string(name) + ": invalid argument");
    io->bitsUsed = used & (ShortBits - 1);
    io->current = current;
    io->accumulator = LoadBigEndian(current) << io->bitsUsed;
}

std::uint32_t TakeBits(Jxr::BitIo* io, std::uint32_t bits, const char* name) {
    const std::uint32_t value = PeekBits(io, bits);
    AdvanceBits(io, bits, name);
    return value;
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

std::uint32_t APS5_VABI peekBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    if (!io || bits == 0 || bits > ShortBits) APS5_INVALID_ARG_EX;
    return PeekBits(io, bits);
}

std::uint32_t APS5_VABI flushBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    if (!io || bits > ShortBits) APS5_INVALID_ARG_EX;
    AdvanceBits(io, bits, __func__);
    return 0;
}

std::uint32_t APS5_VABI getBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    if (!io || bits == 0 || bits > ShortBits) APS5_INVALID_ARG_EX;
    return TakeBits(io, bits, __func__);
}

std::uint32_t APS5_VABI getBool16_nid_postfix(Jxr::BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    return TakeBits(io, 1, __func__);
}

std::int32_t APS5_VABI getBit16s_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    if (!io || bits >= ShortBits) APS5_INVALID_ARG_EX;
    const std::uint32_t value = PeekBits(io, bits + 1);
    if (value < 2) {
        AdvanceBits(io, bits, __func__);
        return 0;
    }
    AdvanceBits(io, bits + 1, __func__);
    const auto magnitude = static_cast<std::int32_t>(value >> 1);
    return (value & 1) ? -magnitude : magnitude;
}

std::uint32_t APS5_VABI getBit32_nid_postfix(Jxr::BitIo* io, std::uint32_t bits) {
    if (!io || bits == 0 || bits > LongBits) APS5_INVALID_ARG_EX;
    std::uint32_t value = 0;
    if (bits > ShortBits) {
        value = TakeBits(io, ShortBits, __func__);
        bits -= ShortBits;
        value <<= bits;
    }
    return value | TakeBits(io, bits, __func__);
}

int APS5_VABI AllocateCodingContextDec_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI CloseWS_List_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI CreateWS_List_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI CreateWS_Memory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EOSWS_File_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI EOSWS_Memory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI FreeDescMetadata_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI GetImageDecodeIID_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI GetImageEncodeIID_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI GetPixelFormatFromHash_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI GetPosWS_File_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI GetPosWS_List_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI GetPosWS_Memory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI IDPEmpty_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI InitZigzagScan_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKAllocAligned_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKAlloc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCodecFactory_CreateCodec_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCodecFactory_CreateDecoderFromFile_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCodecFactory_CreateFormatConverter_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateCodecFactory_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateCodecFactory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateFactory_CreateStream_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateFactory_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateFactory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_Convert_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
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

int APS5_VABI PKFreeAligned_nid_postfix() {
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

int APS5_VABI PKImageDecode_GetColorContext_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetDescriptiveMetadata_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetFrameCount_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetPixelFormat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetResolution_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetSize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Initialize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_SelectFrame_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_CreateNewFrame_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Create_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Create_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Initialize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetColorContext_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetDescriptiveMetadata_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetEXIFMetadata_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetGPSInfoMetadata_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetIPTCNAAMetadata_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetPhotoshopMetadata_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetPixelFormat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetResolution_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetSize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetXMPMetadata_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Terminate_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Transcode_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_WritePixels_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_WriteSource_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ReadWS_File_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ReadWS_List_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ReadWS_Memory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI ResetCodingContext_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SetPosWS_File_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SetPosWS_List_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SetPosWS_Memory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI WMPAlloc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI WMPFree_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI WriteWS_File_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI WriteWS_List_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI WriteWS_Memory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI advanceMRPtr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI advanceOneMBRow_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI allocateBitIOInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI allocatePredInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI allocateQuantizer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI allocateTileInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI attachISRead_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI attachISWrite_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI attach_SB_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI checkImageBuffer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI detachISRead_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI detachISWrite_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI detach_SB_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI dquantBits_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI fillToByte_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI flushToByte_SB_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI flushToByte_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI formatQuantizer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI freePredInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI freeQuantizer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI freeTileInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getACPredMode_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getBit16_S_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getBit32_SB_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getByteRead_SB_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getDCACPredMode_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getPosRead_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getSizeRead_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getSizeWrite_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI getTilePos_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI initMRPtr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI initPostProc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI postProcBlock_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI postProcMB_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI putBit16_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI putBit16z_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI putBit32_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI remapQP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI setBitIOPointers_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI setUniformQuantizer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI slideOneMBRow_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI swapMRPtr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI termPostProc_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI updatePostProcInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI updatePredInfo_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI useDCQuantizer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI useLPQuantizer_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

}
