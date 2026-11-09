#include "prx/libSceVideodec2/include/Api.hpp"
#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace SceVideodec {

struct ConfigInfo {
    std::uint64_t thisSize;
    std::uint32_t codecType;
    std::uint32_t profile;
    std::uint32_t maxLevel;
    std::int32_t maxFrameWidth;
    std::int32_t maxFrameHeight;
    std::int32_t maxDpbFrameCount;
    std::uint64_t videodecFlags;
};

}

namespace SceVideodec {

struct ResourceInfo {
    std::uint64_t thisSize;
    std::uint64_t cpuMemorySize;
    void* cpuMemory;
    std::uint64_t cpuGpuMemorySize;
    void* cpuGpuMemory;
    std::uint64_t maxFrameBufferSize;
    std::uint32_t frameBufferAlignment;
};

}

namespace SceVideodec {

struct Ctrl {
    std::uint64_t thisSize;
    void* handle;
    std::uint64_t version;
};

}

namespace SceVideodec {

struct FrameBuffer {
    std::uint64_t thisSize;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
};

}

namespace SceVideodec {

struct AvcInfo {
    std::uint32_t numUnitsInTick;
    std::uint32_t timeScale;
    std::uint8_t fixedFrameRateFlag;
    std::uint8_t aspectRatioIdc;
    std::uint16_t sarWidth;
    std::uint16_t sarHeight;
    std::uint8_t colourPrimaries;
    std::uint8_t transferCharacteristics;
    std::uint8_t matrixCoefficients;
    std::uint8_t videoFullRangeFlag;
    std::uint32_t frameCropLeftOffset;
    std::uint32_t frameCropRightOffset;
    std::uint32_t frameCropTopOffset;
    std::uint32_t frameCropBottomOffset;
};

}

namespace SceVideodec {

union CodecInfo {
    std::uint8_t reserved[64];
    AvcInfo avc;
};

}

namespace SceVideodec {

struct PictureInfo {
    std::uint64_t thisSize;
    std::uint32_t isValid;
    std::uint32_t codecType;
    std::uint32_t frameWidth;
    std::uint32_t framePitch;
    std::uint32_t frameHeight;
    std::uint32_t isErrorPic;
    std::uint64_t ptsData;
    std::uint64_t attachedData;
    CodecInfo codec;
};

}

extern "C" {

int APS5_VABI sceVideodecCreateDecoder(const SceVideodec::ConfigInfo* config, const SceVideodec::ResourceInfo* resource, SceVideodec::Ctrl* ctrl);
int APS5_VABI sceVideodecDecode(SceVideodec::Ctrl* ctrl, const SceVideodec2::InputData* input, SceVideodec::FrameBuffer* frameBuffer, SceVideodec::PictureInfo* picture);
int APS5_VABI sceVideodecDeleteDecoder(SceVideodec::Ctrl* ctrl);
int APS5_VABI sceVideodecFlush(SceVideodec::Ctrl* ctrl, SceVideodec::FrameBuffer* frameBuffer, SceVideodec::PictureInfo* picture);
int APS5_VABI sceVideodecReset(SceVideodec::Ctrl* ctrl);
int APS5_VABI sceVideodecMapMemory(void);
int APS5_VABI sceVideodecQueryResourceInfo(const SceVideodec::ConfigInfo* config, SceVideodec::ResourceInfo* resource);

}
