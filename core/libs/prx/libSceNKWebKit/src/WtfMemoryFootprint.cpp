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
// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/bmalloc/bmalloc/AvailableMemory.cpp

#include "prx/libSceNKWebKit/include/MemoryFootprintExports.hpp"
#include "prx/libc/include/General.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

extern "C" std::size_t APS5_VABI _ZN3WTF15memoryFootprintEv() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "WTF::memoryFootprint: GetProcessMemoryInfo failed");
    return counters.WorkingSetSize;
#elif defined(__linux__)
    std::ifstream status("/proc/self/statm");
    std::size_t virtualPages = 0;
    std::size_t residentPages = 0;
    if (!(status >> virtualPages >> residentPages))
        throw std::runtime_error("WTF::memoryFootprint: cannot read resident pages from /proc/self/statm");
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0)
        throw std::runtime_error("WTF::memoryFootprint: cannot read the host page size");
    if (residentPages > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(pageSize))
        throw std::overflow_error("WTF::memoryFootprint: resident byte count overflow");
    return residentPages * static_cast<std::size_t>(pageSize);
#elif defined(__APPLE__)
    mach_task_basic_info_data_t information{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&information), &count) != KERN_SUCCESS)
        throw std::runtime_error("WTF::memoryFootprint: task_info failed");
    if (information.resident_size > std::numeric_limits<std::size_t>::max())
        throw std::overflow_error("WTF::memoryFootprint: resident byte count overflow");
    return static_cast<std::size_t>(information.resident_size);
#else
    NotImplemented_nid_no_patch(__func__);
#endif
}
