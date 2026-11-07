// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from shadPS4 src/core/libraries/videodec/videodec2.cpp@2183807e592b36d4440384878165b43d930c1dee

#include "prx/libSceVideodec2/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceVideodec2MapDirectMemory(void* decoder, const SceVideodec2::DirectMemoryInfo* memory) {
    if (!decoder) {
        return static_cast<int>(0x811D0103u);
    }
    if (!memory) {
        return static_cast<int>(0x811D0102u);
    }
    if (memory->thisSize != sizeof(SceVideodec2::DirectMemoryInfo)) {
        return static_cast<int>(0x811D0101u);
    }
    if (!memory->memoryAddr || memory->memorySize == 0) {
        return static_cast<int>(0x811D0104u);
    }
    NotImplemented_nid_no_patch(__func__);
}

}
