// Derived from Source/WTF/wtf/RAMSize.cpp and Source/bmalloc/bmalloc/AvailableMemory.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libkernel/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>

namespace {

constexpr int ScPageSize = 47;
constexpr int ScPhysPages = 121;
constexpr std::size_t MB = 1024 * 1024;
constexpr std::size_t AvailableMemoryGuess = 512 * MB;

using SysconfQuery = std::int64_t (*)(int);

std::int64_t GuestSysconf(int name) {
    return sysconf_nid_postfix(name);
}

SysconfQuery g_sysconf = GuestSysconf;
std::mutex g_ramSizeMutex;
bool g_ramSizeComputed = false;
std::size_t g_ramSize = 0;

std::size_t ComputeAvailableMemory() {
    const std::int64_t pages = g_sysconf(ScPhysPages);
    const std::int64_t pageSize = g_sysconf(ScPageSize);
    if (pages == -1 || pageSize == -1) return AvailableMemoryGuess;
    return static_cast<std::size_t>(pages) * static_cast<std::size_t>(pageSize);
}

} // namespace

extern "C" void RamSizeSysconfForTest_nid_no_patch(SysconfQuery query) {
    const std::lock_guard<std::mutex> lock(g_ramSizeMutex);
    g_sysconf = query ? query : GuestSysconf;
    g_ramSizeComputed = false;
}

namespace NKWebKit {

std::size_t RamSize() {
    const std::lock_guard<std::mutex> lock(g_ramSizeMutex);
    if (!g_ramSizeComputed) {
        g_ramSize = ComputeAvailableMemory();
        g_ramSizeComputed = true;
    }
    return g_ramSize;
}

} // namespace NKWebKit
