// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include "SceTypes.hpp"

struct VdecCoreConfigInfo {
    std::uint32_t decoderType;
    std::uint32_t codecType;
    std::int32_t profile;
    std::int32_t maxLevel;
    std::int32_t maxDpbFrameCount;
    std::int32_t maxFrameWidth;
    std::int32_t maxFrameHeight;
    std::int32_t horizontalAlign;
    std::int32_t surfaceFormat;
    std::int32_t reserved0;
    std::uint8_t hevcTierType;
    std::uint8_t frameHeightAlignment;
    std::uint8_t maxBitDepthLuma;
    std::uint8_t maxBitDepthChroma;
    std::int32_t hevcMaxTemporalIdToDecode;
    std::uint8_t gpuLoadLevel;
    std::int32_t reserved1;
    std::int32_t decodePipelineDepth;
    std::int32_t cpuThreadPriority;
    std::int32_t noOptimizeProgressiveVideo;
    std::int32_t extraDpbFrameCount;
    std::int64_t cpuAffinityMask;
    std::int32_t maxPendingSyncCount;
};

static_assert(sizeof(VdecCoreConfigInfo) == 0x58);

struct VdecCoreMemoryInfo {
    std::uint64_t cpuMemorySize;
    void* cpuMemory;
    std::uint64_t cpuGpuMemorySize;
    void* cpuGpuMemory;
    std::uint64_t gpuMemorySize;
    void* gpuMemory;
};

static_assert(sizeof(VdecCoreMemoryInfo) == 0x30);

struct VdecCoreFrameBufferInfo {
    std::uint32_t frameBufferSize;
    std::uint32_t frameBufferAlignment;
    std::uint32_t minFrameBufferCount;
    std::uint32_t horizontalAlignment;
    std::uint32_t frameWidth;
    std::uint32_t frameHeight;
};

static_assert(sizeof(VdecCoreFrameBufferInfo) == 0x18);

struct VdecCoreComputeResourceInfo {
    std::uint64_t memoryType;
    std::uint64_t size;
    std::uint32_t alignment;
    std::uint32_t reserved;
};

static_assert(sizeof(VdecCoreComputeResourceInfo) == 0x18);

struct VdecCoreMemoryBlock {
    void* address;
    std::uint64_t size;
    std::uint32_t alignment;
    std::uint32_t type;
};

static_assert(sizeof(VdecCoreMemoryBlock) == 0x18);

struct VdecCoreFrameBuffer {
    void* frameBuffer;
    std::uint64_t frameBufferSize;
};

static_assert(sizeof(VdecCoreFrameBuffer) == 0x10);

struct VdecCoreInputData {
    const void* auData;
    std::uint64_t auSize;
    std::int64_t pts;
    std::int64_t dts;
    std::uint64_t userData;
    std::uint32_t flags;
    std::uint32_t streamId;
};

static_assert(sizeof(VdecCoreInputData) == 0x30);

struct VdecCorePicture {
    std::uint32_t flags;
    std::uint32_t reserved0;
    std::uint64_t pts;
    std::uint64_t dts;
    std::uint64_t userData;
    void* pictureInfo;
    void* frameBuffer;
    std::uint32_t frameBufferSize;
    std::uint32_t framePitch;
    std::uint64_t reserved1;
};

static_assert(sizeof(VdecCorePicture) == 0x40);

struct VdecCoreHwPictureInfo {
    std::uint32_t codecType;
    std::uint32_t reserved0;
    std::uint64_t userData;
    std::uint64_t pts;
    std::uint64_t dts;
    std::uint32_t valid;
    std::uint32_t reserved1;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t cropLeft;
    std::uint32_t cropRight;
    std::uint32_t cropTop;
    std::uint32_t cropBottom;
    std::uint32_t sarWidth;
    std::uint32_t sarHeight;
    std::uint32_t reserved2;
    std::uint32_t flags;
    std::uint8_t reserved3[0x400 - 0x50];
};

static_assert(sizeof(VdecCoreHwPictureInfo) == 0x400);

struct VdecCoreOutputInfo {
    std::uint32_t frameWidth;
    std::uint32_t framePitch;
    std::uint32_t frameHeight;
    std::uint32_t pictureCount;
    void* frameBuffer;
    std::uint32_t frameFormat;
    std::uint32_t reserved0;
    VdecCorePicture pictures[2];
    std::uint32_t aspectRatio;
    std::uint32_t sarWidth;
    std::uint32_t videoFormat;
    std::uint32_t colourPrimaries;
    std::uint32_t transferCharacteristics;
    std::uint32_t framePitchInBytes;
    void* lumaBuffer;
    void* chromaBuffer;
    std::uint32_t videoFullRangeFlag;
    std::uint32_t colourDescriptionPresentFlag;
};

static_assert(sizeof(VdecCoreOutputInfo) == 0xd0);

extern "C" {

int APS5_VABI sceVdecCoreQueryMemorySizeSvp();

int APS5_VABI sceVdecCoreAnalyzeStream();
int APS5_VABI sceVdecCoreCreateDecoder(const VdecCoreConfigInfo* config, const VdecCoreMemoryInfo* memory, void* computeQueue, void** decoder);
int APS5_VABI sceVdecCoreDeleteDecoder(void* decoder);
int APS5_VABI sceVdecCoreFinalizeComputeResource();
int APS5_VABI sceVdecCoreFlushDecodeOutput(void* decoder, std::uint32_t* count, void* reserved);
int APS5_VABI sceVdecCoreGetDecodeOutput(void* decoder, VdecCoreFrameBuffer* frameBuffer, std::uint32_t flags, VdecCoreOutputInfo* output, void* reserved);
int APS5_VABI sceVdecCoreInitializeComputeResource();
int APS5_VABI sceVdecCoreMapMemoryBlock(void* decoder, const VdecCoreMemoryBlock* blocks, std::uint32_t count);
int APS5_VABI sceVdecCoreQueryComputeResourceInfo(VdecCoreComputeResourceInfo* info);
int APS5_VABI sceVdecCoreQueryFrameBufferInfo(const VdecCoreConfigInfo* config, VdecCoreFrameBufferInfo* info);
int APS5_VABI sceVdecCoreQueryInstanceSize(const VdecCoreConfigInfo* config, std::uint64_t* instanceSize, std::uint64_t* computeSize, std::uint64_t* reservedSize);
int APS5_VABI sceVdecCoreResetDecoder(void* decoder, void* resetParam);
int APS5_VABI sceVdecCoreSetDecodeInput(void* decoder, const VdecCoreInputData* input, void* reserved, std::uint32_t* status);
int APS5_VABI sceVdecCoreSetDecodeOutputSw();
int APS5_VABI sceVdecCoreSyncDecode(void* decoder, std::uint32_t* count);
int APS5_VABI sceVdecCoreSyncDecodeOutputSw();
int APS5_VABI sceVdecCoreSyncDecodeWptr(void* decoder, std::uint32_t* count, void* writePointer, void* readPointer);
int APS5_VABI sceVdecCoreTrySyncDecode(void* decoder, std::uint32_t* count);
int APS5_VABI sceVdecCoreTrySyncDecodeOutputSw();
int APS5_VABI sceVdecCoreTrySyncDecodeWptr(void* decoder, std::uint32_t* count, void* writePointer, void* readPointer);

}
