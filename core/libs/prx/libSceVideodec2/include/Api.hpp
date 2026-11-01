#pragma once

#include "SceTypes.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <vector>

namespace SceVideodec2 {

struct ComputeMemoryInfo {
    std::uint64_t thisSize;
    std::uint64_t cpuGpuMemorySize;
    void* cpuGpuMemory;
};

}

namespace SceVideodec2 {

struct ComputeConfigInfo {
    std::uint64_t thisSize;
    std::uint16_t computePipeId;
    std::uint16_t computeQueueId;
    bool checkMemoryType;
};

}

namespace SceVideodec2 {

struct DecoderConfigInfo {
    std::uint64_t thisSize;
    std::uint32_t resourceType;
    std::uint32_t codecType;
    std::uint32_t profile;
    std::uint32_t maxLevel;
    std::int32_t maxFrameWidth;
    std::int32_t maxFrameHeight;
    std::int32_t maxDpbFrameCount;
    std::uint32_t decodePipelineDepth;
};

}

namespace SceVideodec2 {

struct DecoderMemoryInfo {
    std::uint64_t thisSize;
    std::uint64_t cpuMemorySize;
    void* cpuMemory;
    std::uint64_t gpuMemorySize;
    void* gpuMemory;
    std::uint64_t cpuGpuMemorySize;
    void* cpuGpuMemory;
    std::uint64_t maxFrameBufferSize;
    std::uint32_t frameBufferAlignment;
};

}

namespace SceVideodec2 {

struct InputData {
    std::uint64_t thisSize;
    const std::uint8_t* auData;
    std::uint64_t auSize;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
};

}

namespace SceVideodec2 {

struct FrameBuffer {
    std::uint64_t thisSize;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
    bool isAccepted;
};

}

namespace SceVideodec2 {

struct OutputInfo {
    std::uint64_t thisSize;
    bool isValid;
    bool isErrorFrame;
    std::uint8_t pictureCount;
    bool isDiscardedFrame;
    std::uint32_t codecType;
    std::uint32_t frameWidth;
    std::uint32_t framePitch;
    std::uint32_t frameHeight;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
    std::uint32_t frameFormat;
    std::uint32_t framePitchInBytes;
};

}

namespace SceVideodec2 {

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
    std::uint8_t aspectRatioInfoPresentFlag;
    std::uint8_t aspectRatioIdc;
    std::uint16_t sarWidth;
    std::uint16_t sarHeight;
    std::uint8_t videoSignalTypePresentFlag;
    std::uint8_t videoFormat;
    std::uint8_t videoFullRangeFlag;
    std::uint8_t colourDescriptionPresentFlag;
    std::uint8_t colourPrimaries;
    std::uint8_t transferCharacteristics;
    std::uint8_t matrixCoefficients;
    std::uint8_t timingInfoPresentFlag;
    std::uint32_t numUnitsInTick;
    std::uint32_t timeScale;
    std::uint8_t fixedFrameRateFlag;
    std::uint8_t bitstreamRestrictionFlag;
    std::uint8_t maxDecFrameBuffering;
    std::uint8_t picStructPresentFlag;
    std::uint8_t picStruct;
    std::uint8_t fieldPicFlag;
    std::uint8_t bottomFieldFlag;
    std::uint8_t sequenceParameterSetPresentFlag;
    std::uint8_t pictureParameterSetPresentFlag;
    std::uint8_t auDelimiterPresentFlag;
    std::uint8_t endOfSequencePresentFlag;
    std::uint8_t endOfStreamPresentFlag;
    std::uint8_t fillerDataPresentFlag;
    std::uint8_t pictureTimingSeiPresentFlag;
    std::uint8_t bufferingPeriodSeiPresentFlag;
    std::uint8_t constraintSet0Flag;
    std::uint8_t constraintSet1Flag;
    std::uint8_t constraintSet2Flag;
    std::uint8_t constraintSet3Flag;
    std::uint8_t constraintSet4Flag;
    std::uint8_t constraintSet5Flag;
};

}

namespace SceVideodec2 {

struct Vp9PictureInfo {
    std::uint64_t thisSize;
    bool isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint32_t profile;
    std::uint32_t level;
    std::uint32_t keyFrameFlag;
    std::uint32_t intraOnly;
    std::uint8_t bitDepth;
    std::uint8_t colorRange;
    std::uint32_t colorSpace;
    std::uint32_t numFrames;
    std::uint32_t frameWidth;
    std::uint32_t frameHeight;
    std::uint32_t renderWidth;
    std::uint32_t renderHeight;
};

}

namespace SceVideodec2 {

struct HevcPictureInfo {
    std::uint64_t thisSize;
    bool isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint32_t width;
    std::uint32_t height;
    std::uint8_t bitDepthLumaMinus8;
    std::uint8_t bitDepthChromaMinus8;
    std::uint8_t timingInfoPresent;
    std::uint32_t numUnitsInTick;
    std::uint32_t timeScale;
    std::uint32_t aspectRatioInfoPresent;
    std::uint8_t aspectRatioIdc;
    std::uint16_t sarWidth;
    std::uint16_t sarHeight;
    std::uint8_t videoSignalTypePresent;
    std::uint8_t videoFormat;
    std::uint8_t fullRange;
    std::uint8_t colourDescriptionPresent;
    std::uint8_t colourPrimaries;
    std::uint8_t transferCharacteristics;
    std::uint8_t matrixCoefficients;
    std::uint8_t frameFieldInfoPresent;
    std::uint32_t picStruct;
    std::uint32_t sourceScanType;
    std::uint32_t duplicate;
    std::uint32_t conformanceWindow;
    std::uint32_t confLeft;
    std::uint32_t confRight;
    std::uint32_t confTop;
    std::uint32_t confBottom;
    std::uint32_t defaultDisplayWindow;
    std::uint32_t displayLeft;
    std::uint32_t displayRight;
    std::uint32_t displayTop;
    std::uint32_t displayBottom;
    std::uint8_t chromaLocInfoPresent;
    std::uint8_t chromaLocTop;
    std::uint8_t chromaLocBottom;
    std::uint8_t fieldSequence;
    std::uint8_t vpsPresent;
    std::uint8_t spsPresent;
    std::uint8_t ppsPresent;
    std::uint8_t delimiterPresent;
    std::uint8_t endSequencePresent;
    std::uint8_t endStreamPresent;
    std::uint8_t fillerPresent;
    std::uint8_t pictureTimingPresent;
    std::uint8_t bufferingPeriodPresent;
    std::uint8_t framePackingPresent;
    std::uint8_t alternativeTransferPresent;
    std::uint8_t idr;
    std::uint8_t irap;
    std::uint8_t profileSpace;
    std::uint8_t tier;
    std::uint8_t profile;
    std::uint8_t progressiveSource;
    std::uint8_t interlacedSource;
    std::uint8_t frameOnlyConstraint;
    std::uint8_t level;
    std::uint8_t subLayerProfilePresent;
    std::uint8_t subLayerLevelPresent;
    std::uint8_t subLayerProfileSpace;
    std::uint8_t subLayerTier;
    std::uint8_t subLayerProfile;
    std::uint8_t subLayerLevel;
    std::uint8_t subLayerOrderingPresent;
    std::uint8_t maxDecPicBufferingMinus1;
    std::uint8_t preferredTransfer;
    std::uint8_t cropping;
    std::uint32_t cropLeft;
    std::uint32_t cropRight;
    std::uint32_t cropTop;
    std::uint32_t cropBottom;
};

}

extern "C" {

int APS5_VABI sceVideodec2QueryComputeMemoryInfo_nid_postfix(SceVideodec2::ComputeMemoryInfo* info);
int APS5_VABI sceVideodec2AllocateComputeQueue_nid_postfix(const SceVideodec2::ComputeConfigInfo* config, const SceVideodec2::ComputeMemoryInfo* memory, std::uint64_t* queue);
int APS5_VABI sceVideodec2ReleaseComputeQueue_nid_postfix(std::uint64_t queue);
int APS5_VABI sceVideodec2QueryDecoderMemoryInfo_nid_postfix(const SceVideodec2::DecoderConfigInfo* config, SceVideodec2::DecoderMemoryInfo* memory);
int APS5_VABI sceVideodec2CreateDecoder_nid_postfix(const SceVideodec2::DecoderConfigInfo* config, const SceVideodec2::DecoderMemoryInfo* memory, std::uint64_t* handle);
int APS5_VABI sceVideodec2DeleteDecoder_nid_postfix(std::uint64_t handle);
int APS5_VABI sceVideodec2Decode_nid_postfix(std::uint64_t handle, const SceVideodec2::InputData* input, SceVideodec2::FrameBuffer* frame, SceVideodec2::OutputInfo* output);
int APS5_VABI sceVideodec2Flush_nid_postfix(std::uint64_t handle, SceVideodec2::FrameBuffer* frame, SceVideodec2::OutputInfo* output);
int APS5_VABI sceVideodec2Reset_nid_postfix(std::uint64_t handle);
int APS5_VABI sceVideodec2GetPictureInfo_nid_postfix(const SceVideodec2::OutputInfo* output, SceVideodec2::AvcPictureInfo* first, void* second);
int APS5_VABI sceVideodec2GetAvcPictureInfo_nid_postfix(const SceVideodec2::OutputInfo* output, SceVideodec2::AvcPictureInfo* first, SceVideodec2::AvcPictureInfo* second);
int APS5_VABI sceVideodec2GetVp9PictureInfo_nid_postfix(const SceVideodec2::OutputInfo* output, SceVideodec2::Vp9PictureInfo* info);
int APS5_VABI sceVideodec2TermLibrary();
int APS5_VABI sceVideodec2QueryLibraryInfo();
int APS5_VABI sceVideodec2QueryDecoderInfo();
int APS5_VABI sceVideodec2MapMemory();
int APS5_VABI sceVideodec2MapDirectMemory(void*, const void*);
int APS5_VABI sceVideodec2PresetDecoderConfigInfo();
int APS5_VABI sceVideodec2PresetDecoderConfigInfoAvc();
int APS5_VABI sceVideodec2PresetDecoderConfigInfoHevc();
int APS5_VABI sceVideodec2PresetDecoderConfigInfoVp9();
int APS5_VABI sceVideodec2CreateDecoderBid();
int APS5_VABI sceVideodec2CreateHevcDecoder();
int APS5_VABI sceVideodec2QueryHevcDecoderMemoryInfo();
int APS5_VABI sceVideodec2GetHevcPictureInfo_nid_postfix(const SceVideodec2::OutputInfo* output, SceVideodec2::HevcPictureInfo* info);
int APS5_VABI sceVideodec2InitLibrary();

}
