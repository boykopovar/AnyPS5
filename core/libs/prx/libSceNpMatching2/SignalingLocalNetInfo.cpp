// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from shadPS4 src/core/libraries/np/np_matching2/np_matching2.cpp@2183807e592b36d4440384878165b43d930c1dee and src/core/libraries/np/np_matching2/np_matching2_signaling.cpp@2183807e592b36d4440384878165b43d930c1dee

#include "prx/libSceNpMatching2/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>

namespace {

constexpr int SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED = static_cast<int>(0x80550C03);
constexpr int SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550C0A);
constexpr std::uint64_t SIGNALING_NET_INFO_SIZE = 0x18;

static_assert(sizeof(NpMatching2SignalingNetInfo) == SIGNALING_NET_INFO_SIZE);
static_assert(offsetof(NpMatching2SignalingNetInfo, size) == 0x00);
static_assert(offsetof(NpMatching2SignalingNetInfo, localAddr) == 0x08);
static_assert(offsetof(NpMatching2SignalingNetInfo, mappedAddr) == 0x0C);
static_assert(offsetof(NpMatching2SignalingNetInfo, natStatus) == 0x10);
static_assert(offsetof(NpMatching2SignalingNetInfo, padding) == 0x14);

}

extern "C" bool NpMatching2IsInitialized_nid_no_patch();

extern "C" {

int APS5_VABI sceNpMatching2SignalingGetLocalNetInfo(NpMatching2SignalingNetInfo* info) {
    if (!NpMatching2IsInitialized_nid_no_patch()) {
        return SCE_NP_MATCHING2_ERROR_NOT_INITIALIZED;
    }
    if (!info || info->size != SIGNALING_NET_INFO_SIZE) {
        return SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
