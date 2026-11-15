#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include "prx/libSceJxr/GuestPixelInfo.h"
#include "prx/libSceJxr/ImageEncode.hpp"
#include "prx/libSceJxr/Stream.hpp"
#include "prx/libSceJxr/include/GuidExports.hpp"

struct tagPKImageEncode;
struct JxrGuestPostProcInfo {
    std::int32_t mbDc;
    std::uint8_t mbTexture;
    std::int32_t blockDc[4][4];
    std::uint8_t blockTexture[4][4];
};

static_assert(offsetof(JxrGuestPostProcInfo, mbTexture) == 4);
static_assert(offsetof(JxrGuestPostProcInfo, blockDc) == 8);
static_assert(offsetof(JxrGuestPostProcInfo, blockTexture) == 72);
static_assert(sizeof(JxrGuestPostProcInfo) == 88);

extern "C" {

std::int64_t APS5_VABI PKFree_nid_postfix(void** memory);
std::int64_t APS5_VABI PixelFormatLookup_nid_postfix(JxrPixelInfo* value, std::uint8_t mode);
std::int64_t APS5_VABI readIS_nid_postfix(void* codec, Jxr::BitIo* io);
std::int64_t APS5_VABI writeIS_nid_postfix(void* codec, Jxr::BitIo* io);
std::int64_t APS5_VABI attach_SB_nid_postfix(Jxr::SimpleBitIo* io, Jxr::Stream* stream);
std::uint32_t APS5_VABI getBit32_SB_nid_postfix(Jxr::SimpleBitIo* io, std::uint32_t bits);
void APS5_VABI flushToByte_SB_nid_postfix(Jxr::SimpleBitIo* io);
std::uint32_t APS5_VABI getByteRead_SB_nid_postfix(Jxr::SimpleBitIo* io);
std::int64_t APS5_VABI detach_SB_nid_postfix(Jxr::SimpleBitIo* io);
std::uint32_t APS5_VABI peekBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits);
std::uint32_t APS5_VABI flushBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits);
std::uint32_t APS5_VABI getBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t bits);
std::uint32_t APS5_VABI getBool16_nid_postfix(Jxr::BitIo* io);
std::int32_t APS5_VABI getBit16s_nid_postfix(Jxr::BitIo* io, std::uint32_t bits);
std::uint32_t APS5_VABI getBit32_nid_postfix(Jxr::BitIo* io, std::uint32_t bits);
std::uint32_t APS5_VABI flushToByte_nid_postfix(Jxr::BitIo* io);
void APS5_VABI putBit16z_nid_postfix(Jxr::BitIo* io, std::uint32_t value, std::uint32_t bits);
void APS5_VABI putBit16_nid_postfix(Jxr::BitIo* io, std::uint32_t value, std::uint32_t bits);
void APS5_VABI putBit32_nid_postfix(Jxr::BitIo* io, std::uint32_t value, std::uint32_t bits);
void APS5_VABI fillToByte_nid_postfix(Jxr::BitIo* io);
std::uint32_t APS5_VABI getBit16_S_nid_postfix(void* codec, Jxr::BitIo* io, std::uint32_t bits);
std::uint32_t APS5_VABI getSizeRead_nid_postfix(Jxr::BitIo* io);
std::uint32_t APS5_VABI getSizeWrite_nid_postfix(Jxr::BitIo* io);
std::uint32_t APS5_VABI getPosRead_nid_postfix(Jxr::BitIo* io);
std::uint8_t APS5_VABI dquantBits_nid_postfix(std::uint8_t qp);
std::int64_t APS5_VABI GetImageDecodeIID_nid_postfix(const char* extension, const std::uint32_t** iid);
std::int64_t APS5_VABI GetImageEncodeIID_nid_postfix(const char* extension, const std::uint32_t** iid);
const void* APS5_VABI GetPixelFormatFromHash_nid_postfix(std::uint8_t hash);
std::int32_t APS5_VABI IDPEmpty_nid_postfix(void* codec);
std::int32_t APS5_VABI setBitIOPointers_nid_postfix(void* codec);
std::int64_t APS5_VABI PKAllocAligned_nid_postfix(void** memory, std::size_t size, std::size_t alignment);
std::int64_t APS5_VABI PKFreeAligned_nid_postfix(void** memory);
void APS5_VABI remapQP_nid_postfix(void* quantizer, std::int32_t shift, std::int32_t scaled);
void APS5_VABI formatQuantizer_nid_postfix(void** quantizers, std::uint8_t mode, std::size_t channels,
                                           std::size_t position, std::int32_t shiftedUv, std::int32_t scaled);
std::int32_t APS5_VABI getACPredMode_nid_postfix(void* macroblock, std::int32_t format);
void APS5_VABI InitZigzagScan_nid_postfix(void* context);
void APS5_VABI freeQuantizer_nid_postfix(void** quantizers);
void APS5_VABI setUniformQuantizer_nid_postfix(void* codec, std::size_t band);
void APS5_VABI useDCQuantizer_nid_postfix(void* codec, std::size_t tile);
void APS5_VABI useLPQuantizer_nid_postfix(void* codec, std::size_t count, std::size_t tile);
std::int32_t APS5_VABI getDCACPredMode_nid_postfix(void* codec, std::size_t column);
void APS5_VABI updatePredInfo_nid_postfix(void* codec, void* macroblock, std::size_t column, std::int32_t format);
std::int32_t APS5_VABI allocatePredInfo_nid_postfix(void* codec);
void APS5_VABI freePredInfo_nid_postfix(void* codec);

std::int32_t APS5_VABI AllocateCodingContextDec_nid_postfix(void* codec, std::int32_t count);
std::int64_t APS5_VABI CloseWS_List_nid_postfix(Jxr::Stream** stream);
std::int64_t APS5_VABI CreateWS_Memory_nid_postfix(Jxr::Stream** stream, void* buffer, std::size_t size);
std::int64_t APS5_VABI CreateWS_List_nid_postfix(Jxr::Stream** stream);
int APS5_VABI EOSWS_File_nid_postfix(Jxr::Stream* stream);
std::int32_t APS5_VABI EOSWS_Memory_nid_postfix(Jxr::Stream* stream);
int APS5_VABI FreeDescMetadata_nid_postfix();
std::int64_t APS5_VABI GetPosWS_File_nid_postfix(Jxr::Stream* stream, std::size_t* position);
std::int64_t APS5_VABI GetPosWS_Memory_nid_postfix(Jxr::Stream* stream, std::size_t* position);
std::int64_t APS5_VABI GetPosWS_List_nid_postfix(Jxr::Stream* stream, std::size_t* position);
std::int64_t APS5_VABI PKAlloc_nid_postfix(void** memory, std::size_t size);
std::int64_t APS5_VABI PKCodecFactory_CreateCodec_nid_postfix(const std::uint32_t* iid, void** codec);
int APS5_VABI PKCodecFactory_CreateDecoderFromFile_nid_postfix();
std::int64_t APS5_VABI PKCodecFactory_CreateFormatConverter_nid_postfix(void** converter);
std::int64_t APS5_VABI PKCreateCodecFactory_Release_nid_postfix(void** factory);
std::int64_t APS5_VABI PKCreateCodecFactory_nid_postfix(void** factory, std::uint32_t version);
std::int64_t APS5_VABI PKCreateFactory_CreateStream_nid_postfix(void** stream);
std::int64_t APS5_VABI PKCreateFactory_Release_nid_postfix(void** factory);
std::int64_t APS5_VABI PKCreateFactory_nid_postfix(void** factory, std::uint32_t version);
std::int64_t APS5_VABI PKFormatConverter_Convert_nid_postfix(void* converter, const void* rect, std::uint8_t* pixels, std::uint32_t stride);
int APS5_VABI PKFormatConverter_Copy_nid_postfix();
int APS5_VABI PKFormatConverter_EnumConversions_nid_postfix();
int APS5_VABI PKFormatConverter_GetPixelFormat_nid_postfix();
int APS5_VABI PKFormatConverter_GetResolution_nid_postfix();
int APS5_VABI PKFormatConverter_GetSize_nid_postfix();
int APS5_VABI PKFormatConverter_GetSourcePixelFormat_nid_postfix();
int APS5_VABI PKFormatConverter_InitializeConvert_nid_postfix();
int APS5_VABI PKFormatConverter_Initialize_nid_postfix();
int APS5_VABI PKFormatConverter_Release_nid_postfix();
std::int64_t APS5_VABI PKImageDecode_Copy_nid_postfix(void* decoder, const void* rect, std::uint8_t* buffer, std::uint32_t stride);
int APS5_VABI PKImageDecode_Create_WMP_nid_postfix();
std::int64_t APS5_VABI PKImageDecode_Create_nid_postfix(void** decoder);
std::int64_t APS5_VABI PKImageDecode_GetColorContext_nid_postfix(void* decoder, std::uint8_t* buffer, std::uint32_t* size);
std::int64_t APS5_VABI PKImageDecode_GetDescriptiveMetadata_nid_postfix(void* decoder, void* metadata);
std::int64_t APS5_VABI PKImageDecode_GetFrameCount_nid_postfix(void* decoder, std::uint32_t* count);
std::int64_t APS5_VABI PKImageDecode_GetPixelFormat_nid_postfix(void* decoder, JxrGuestGuid* pixelFormat);
std::int64_t APS5_VABI PKImageDecode_GetResolution_nid_postfix(void* decoder, float* resolutionX, float* resolutionY);
std::int64_t APS5_VABI PKImageDecode_GetSize_nid_postfix(void* decoder, std::int32_t* width, std::int32_t* height);
std::int64_t APS5_VABI PKImageDecode_Initialize_nid_postfix(void* decoder, Jxr::Stream* stream);
std::int64_t APS5_VABI PKImageDecode_Release_nid_postfix(void** decoder);
std::int64_t APS5_VABI PKImageDecode_SelectFrame_nid_postfix(void* decoder, std::uint32_t frame);
std::int64_t APS5_VABI PKImageEncode_CreateNewFrame_nid_postfix(void* encoder, void* param, std::size_t paramSize);
int APS5_VABI PKImageEncode_Create_WMP_nid_postfix();
std::int64_t APS5_VABI PKImageEncode_Create_nid_postfix(Jxr::ImageEncode** encoder);
std::int64_t APS5_VABI PKImageEncode_Initialize_nid_postfix(Jxr::ImageEncode* encoder, Jxr::Stream* stream, void* parameter, std::size_t size);
std::int64_t APS5_VABI PKImageEncode_Release_nid_postfix(Jxr::ImageEncode** encoder);
std::int64_t APS5_VABI PKImageEncode_SetColorContext_nid_postfix(Jxr::ImageEncode* encoder, const std::uint8_t* context, std::uint32_t size);
std::int64_t APS5_VABI PKImageEncode_SetDescriptiveMetadata_nid_postfix(Jxr::ImageEncode* encoder, const void* metadata);
std::int64_t APS5_VABI PKImageEncode_SetEXIFMetadata_WMP_nid_postfix(
    tagPKImageEncode* encoder, const std::uint8_t* metadata, std::uint32_t size);
std::int64_t APS5_VABI PKImageEncode_SetGPSInfoMetadata_WMP_nid_postfix(
    tagPKImageEncode* encoder, const std::uint8_t* metadata, std::uint32_t size);
std::int64_t APS5_VABI PKImageEncode_SetIPTCNAAMetadata_WMP_nid_postfix(
    tagPKImageEncode* encoder, const std::uint8_t* metadata, std::uint32_t size);
std::int64_t APS5_VABI PKImageEncode_SetPhotoshopMetadata_WMP_nid_postfix(
    tagPKImageEncode* encoder, const std::uint8_t* metadata, std::uint32_t size);
std::int64_t APS5_VABI PKImageEncode_SetPixelFormat_nid_postfix(Jxr::ImageEncode* encoder, JxrGuestGuid format);
std::int64_t APS5_VABI PKImageEncode_SetResolution_nid_postfix(Jxr::ImageEncode* encoder, float horizontal, float vertical);
std::int64_t APS5_VABI PKImageEncode_SetSize_nid_postfix(Jxr::ImageEncode* encoder, std::int32_t width, std::int32_t height);
std::int64_t APS5_VABI PKImageEncode_SetXMPMetadata_WMP_nid_postfix(tagPKImageEncode* encoder,
                                                                   const std::uint8_t* metadata, std::uint32_t size);
std::int64_t APS5_VABI PKImageEncode_Terminate_nid_postfix(void* encoder);
std::int64_t APS5_VABI PKImageEncode_Transcode_nid_postfix(void* encoder, void* converter, void* rectangle);
std::int64_t APS5_VABI PKImageEncode_WritePixels_nid_postfix(Jxr::ImageEncode* encoder, std::uint32_t lines, std::uint8_t* pixels, std::uint32_t stride);
std::int64_t APS5_VABI PKImageEncode_WriteSource_nid_postfix(void* encoder, void* converter, void* rectangle);
std::int64_t APS5_VABI ReadWS_File_nid_postfix(Jxr::Stream* stream, void* data, std::size_t size);
std::int64_t APS5_VABI ReadWS_Memory_nid_postfix(Jxr::Stream* stream, void* buffer, std::size_t size);
std::int64_t APS5_VABI ReadWS_List_nid_postfix(Jxr::Stream* stream, void* data, std::size_t size);
void APS5_VABI ResetCodingContext_nid_postfix(void* context);
std::int64_t APS5_VABI SetPosWS_File_nid_postfix(Jxr::Stream* stream, std::size_t position);
std::int64_t APS5_VABI SetPosWS_Memory_nid_postfix(Jxr::Stream* stream, std::size_t position);
std::int64_t APS5_VABI WMPAlloc_nid_postfix(void** memory, std::size_t size);
std::int64_t APS5_VABI WMPFree_nid_postfix(void** memory);
std::int64_t APS5_VABI SetPosWS_List_nid_postfix(Jxr::Stream* stream, std::size_t position);
std::int64_t APS5_VABI WriteWS_File_nid_postfix(Jxr::Stream* stream, const void* data, std::size_t size);
std::int64_t APS5_VABI WriteWS_Memory_nid_postfix(Jxr::Stream* stream, const void* buffer, std::size_t size);
std::int64_t APS5_VABI WriteWS_List_nid_postfix(Jxr::Stream* stream, const void* data, std::size_t size);
void APS5_VABI advanceMRPtr_nid_postfix(void* codec);
void APS5_VABI advanceOneMBRow_nid_postfix(void* codec);
std::int32_t APS5_VABI allocateBitIOInfo_nid_postfix(void* codec);
int APS5_VABI allocateQuantizer_nid_postfix(void** quantizers, std::size_t channels, std::size_t quantizerCount);
std::int32_t APS5_VABI allocateTileInfo_nid_postfix(void* codec);
std::int64_t APS5_VABI attachISRead_nid_postfix(Jxr::BitIo* io, Jxr::Stream* stream, void* codec);
std::int64_t APS5_VABI attachISWrite_nid_postfix(Jxr::BitIo* io, Jxr::Stream* stream);
std::int32_t APS5_VABI checkImageBuffer_nid_postfix(void* codec, std::size_t width, std::size_t rows);
std::int64_t APS5_VABI detachISRead_nid_postfix(void* codec, Jxr::BitIo* io);
std::int64_t APS5_VABI detachISWrite_nid_postfix(void* codec, Jxr::BitIo* io);
void APS5_VABI freeTileInfo_nid_postfix(void* codec);
void APS5_VABI getTilePos_nid_postfix(void* codec, std::size_t column, std::size_t row);
void APS5_VABI initMRPtr_nid_postfix(void* codec);
std::int32_t APS5_VABI initPostProc_nid_postfix(JxrGuestPostProcInfo* (*postProcInfo)[2], std::size_t mbWidth, std::size_t numChannels);
void APS5_VABI postProcBlock_nid_postfix(void* table, std::int32_t* previous, std::int32_t* current, std::size_t column,
                                         std::size_t channel, std::int32_t threshold);
void APS5_VABI postProcMB_nid_postfix(void* table, std::int32_t* previous, std::int32_t* current, std::size_t column,
                                      std::size_t channel, std::int32_t threshold);
void APS5_VABI slideOneMBRow_nid_postfix(void* table, std::size_t channels, std::size_t width, std::int32_t top,
                                         std::int32_t bottom);
void APS5_VABI swapMRPtr_nid_postfix(void* codec);
void APS5_VABI termPostProc_nid_postfix(JxrGuestPostProcInfo* (*postProcInfo)[2], std::size_t numChannels);
void APS5_VABI updatePostProcInfo_nid_postfix(void* table, std::int32_t* macroblock, std::size_t column,
                                              std::size_t channel);

}
