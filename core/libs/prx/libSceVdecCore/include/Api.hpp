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

extern "C" {

int APS5_VABI sceVdecCoreQueryMemorySizeSvp();

int APS5_VABI sceVdecCoreAnalyzeStream();
int APS5_VABI sceVdecCoreCreateDecoder(const VdecCoreConfigInfo* config, const VdecCoreMemoryInfo* memory, void* computeQueue, void** decoder);
int APS5_VABI sceVdecCoreDeleteDecoder(void* decoder);
int APS5_VABI sceVdecCoreFinalizeComputeResource();
int APS5_VABI sceVdecCoreFlushDecodeOutput();
int APS5_VABI sceVdecCoreGetDecodeOutput();
int APS5_VABI sceVdecCoreInitializeComputeResource();
int APS5_VABI sceVdecCoreMapMemoryBlock(void* decoder, const VdecCoreMemoryBlock* blocks, std::uint32_t count);
int APS5_VABI sceVdecCoreQueryComputeResourceInfo(VdecCoreComputeResourceInfo* info);
int APS5_VABI sceVdecCoreQueryFrameBufferInfo(const VdecCoreConfigInfo* config, VdecCoreFrameBufferInfo* info);
int APS5_VABI sceVdecCoreQueryInstanceSize(const VdecCoreConfigInfo* config, std::uint64_t* instanceSize, std::uint64_t* computeSize, std::uint64_t* reservedSize);
int APS5_VABI sceVdecCoreResetDecoder();
int APS5_VABI sceVdecCoreSetDecodeInput();
int APS5_VABI sceVdecCoreSetDecodeOutputSw();
int APS5_VABI sceVdecCoreSyncDecode();
int APS5_VABI sceVdecCoreSyncDecodeOutputSw();
int APS5_VABI sceVdecCoreSyncDecodeWptr();
int APS5_VABI sceVdecCoreTrySyncDecode();
int APS5_VABI sceVdecCoreTrySyncDecodeOutputSw();
int APS5_VABI sceVdecCoreTrySyncDecodeWptr();

}
