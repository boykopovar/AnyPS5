#include "prx/libSceVideodec2/include/Api.hpp"
#pragma once

#include "SceTypes.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <variant>
#include <vector>

namespace SceVdecsw {

struct DecoderConfigInfo {
    SceVideodec2::DecoderConfigInfo decoder;
    std::uint64_t computeQueue;
    std::uint64_t cpuAffinityMask;
    std::int32_t cpuThreadPriority;
    bool optimizeProgressiveVideo;
    std::uint8_t reserved[19];
};

struct InputResult {
    std::uint64_t thisSize;
    const void* decodedAu;
    std::uint32_t outputFrameCount;
    std::uint32_t reserved;
};

}

namespace SceVdecsw {

struct FrameBuffer {
    std::uint64_t thisSize;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
};

}

namespace SceVdecsw {

struct OutputInfo {
    std::uint64_t thisSize;
    bool isValid;
    bool isLastFrame;
    bool isErrorFrame;
    std::uint8_t pictureCount;
    std::uint32_t codecType;
    std::uint32_t frameWidth;
    std::uint32_t framePitch;
    std::uint32_t frameHeight;
    bool isDiscardedFrame;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
    std::uint32_t frameFormat;
    std::uint32_t framePitchInBytes;
};

}

namespace SceVdecsw {

struct AvcPictureInfo {
    std::uint64_t thisSize;
    bool isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint8_t idrPictureFlag;
    std::uint8_t profileIdc;
    std::uint8_t levelIdc;
    std::uint32_t picWidthInMbsMinus1;
    std::uint32_t picHeightInMapUnitsMinus1;
    std::uint8_t frameMbsOnlyFlag;
    std::uint8_t frameCroppingFlag;
    std::uint32_t frameCropLeftOffset;
    std::uint32_t frameCropRightOffset;
    std::uint32_t frameCropTopOffset;
    std::uint32_t frameCropBottomOffset;
    std::uint8_t vui[44];
};

}

namespace SceVdecsw {

struct HevcPictureInfo {
    std::uint64_t thisSize;
    bool isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint32_t picWidthInLumaSamples;
    std::uint32_t picHeightInLumaSamples;
    std::uint8_t bitDepthLumaMinus8;
    std::uint8_t bitDepthChromaMinus8;
    std::uint8_t timingInfoPresentFlag;
    std::uint32_t numUnitsInTick;
    std::uint32_t timeScale;
    std::uint32_t aspectRatioInfoPresentFlag;
    std::uint8_t aspectRatioIdc;
    std::uint16_t sarWidth;
    std::uint16_t sarHeight;
    std::uint8_t videoSignal[8];
    std::uint32_t picStruct;
    std::uint32_t sourceScanType;
    std::uint32_t duplicateFlag;
    std::uint32_t conformanceWindowFlag;
    std::uint32_t confWinLeftOffset;
    std::uint32_t confWinRightOffset;
    std::uint32_t confWinTopOffset;
    std::uint32_t confWinBottomOffset;
    std::uint32_t defaultDisplayWindow[5];
    std::uint8_t chromaLocation[4];
    std::uint8_t presentFlags[11];
    std::uint8_t idrPictureFlag;
    std::uint8_t irapPictureFlag;
    std::uint8_t generalProfileSpace;
    std::uint8_t generalTierFlag;
    std::uint8_t generalProfileIdc;
    std::uint8_t generalSourceFlags[3];
    std::uint8_t generalLevelIdc;
    std::uint8_t subLayer[9];
    std::uint8_t frameCroppingFlag;
    std::uint32_t frameCropLeftOffset;
    std::uint32_t frameCropRightOffset;
    std::uint32_t frameCropTopOffset;
    std::uint32_t frameCropBottomOffset;
};

}

extern "C" {

int APS5_VABI sceVdecswQueryComputeMemoryInfo(SceVideodec2::ComputeMemoryInfo* info);
int APS5_VABI sceVdecswAllocateComputeQueue(const SceVideodec2::ComputeConfigInfo* config, const SceVideodec2::ComputeMemoryInfo* memory, std::uint64_t* queue);
int APS5_VABI sceVdecswReleaseComputeQueue(std::uint64_t queue);
int APS5_VABI sceVdecswQueryDecoderMemoryInfo(const void* config, SceVideodec2::DecoderMemoryInfo* memory);
int APS5_VABI sceVdecswCreateDecoder(const void* config, const SceVideodec2::DecoderMemoryInfo* memory, std::uint64_t* handle);
int APS5_VABI sceVdecswDeleteDecoder(std::uint64_t handle);
int APS5_VABI sceVdecswSetDecodeInput(std::uint64_t handle, const SceVideodec2::InputData* input);
int APS5_VABI sceVdecswTrySyncDecodeInput(std::uint64_t handle, SceVdecsw::InputResult* result);
int APS5_VABI sceVdecswSyncDecodeInput(std::uint64_t handle, SceVdecsw::InputResult* result);
int APS5_VABI sceVdecswSetDecodeOutput(std::uint64_t handle, const SceVdecsw::FrameBuffer* frame);
int APS5_VABI sceVdecswTrySyncDecodeOutput(std::uint64_t handle, SceVdecsw::OutputInfo* output);
int APS5_VABI sceVdecswSyncDecodeOutput(std::uint64_t handle, SceVdecsw::OutputInfo* output);
int APS5_VABI sceVdecswGetPictureInfo(const SceVdecsw::OutputInfo* output, void* first, void* second);
int APS5_VABI sceVdecswGetAvcPictureInfo(const SceVdecsw::OutputInfo* output, SceVdecsw::AvcPictureInfo* first, SceVdecsw::AvcPictureInfo* second);
int APS5_VABI sceVdecswGetHevcPictureInfo(const SceVdecsw::OutputInfo* output, SceVdecsw::HevcPictureInfo* picture);
int APS5_VABI sceVdecswGetVp9PictureInfo(const SceVdecsw::OutputInfo* output, SceVideodec2::Vp9PictureInfo* picture);
int APS5_VABI sceVdecswFinalizeDecodeSequence(std::uint64_t handle);
int APS5_VABI sceVdecswResetDecoder(std::uint64_t handle);

}
