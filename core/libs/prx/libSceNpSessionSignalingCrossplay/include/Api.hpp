#pragma once

#include <cstdint>
#include "SceTypes.hpp"

struct NpSessionSignalingCrossplayInitParam {
    std::uint32_t reserved0;
    std::uint64_t heapSize;
    std::uint64_t reserved1;
    std::uint32_t reserved2;
    std::uint64_t reserved3;
};

struct NpSessionSignalingCrossplayMemoryInfo {
    std::uint64_t poolSize;
    std::uint64_t currentInuseSize;
    std::uint64_t maxInuseSize;
};

extern "C" {

int APS5_VABI sceNpSessionSignalingCrossplayInitialize(const NpSessionSignalingCrossplayInitParam* parameter);
int APS5_VABI sceNpSessionSignalingCrossplayTerminate(void);
int APS5_VABI sceNpSessionSignalingCrossplayCreateContext(void);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown00(void);
int APS5_VABI sceNpSessionSignalingCrossplayDestroyContext(void);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown01(void);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown02(std::uint64_t argument1, const void* argument2, std::uint64_t argument3, const void* argument4, void* argument5);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown03(void);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown04(void);
int APS5_VABI sceNpSessionSignalingCrossplayUnknown05(void);
int APS5_VABI sceNpSessionSignalingCrossplayGetMemoryInfo(NpSessionSignalingCrossplayMemoryInfo* info);

}
