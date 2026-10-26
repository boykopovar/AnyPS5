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
    std::uint32_t picWidthInLumaSamples;
    std::uint32_t picHeightInLumaSamples;
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
int APS5_VABI sceVideodec2GetHevcPictureInfo();
int APS5_VABI sceVideodec2InitLibrary();

}
