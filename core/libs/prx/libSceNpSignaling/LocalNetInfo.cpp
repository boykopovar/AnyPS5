// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from shadPS4 src/core/libraries/np/np_signaling/np_signaling.cpp@2183807e592b36d4440384878165b43d930c1dee

#include "prx/libSceNpSignaling/include/Api.hpp"
#include "prx/libSceNpSignaling/include/SignalingState.hpp"
#include <cerrno>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED = static_cast<int>(0x80552701);
static constexpr int SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80552715);

extern "C" {

int APS5_VABI sceNpSignalingGetLocalNetInfo(int ctxId, NpSignalingNetInfo* info) {
    (void)ctxId;
    if (!NpSignalingIsInitialized_nid_no_patch()) {
        return SCE_NP_SIGNALING_ERROR_NOT_INITIALIZED;
    }
    if (!info) {
        return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    }
    std::uint32_t size = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&size, &info->size, sizeof(size));
    errno = savedErrno;
    if (!copied || size != sizeof(NpSignalingNetInfo)) {
        return SCE_NP_SIGNALING_ERROR_INVALID_ARGUMENT;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
