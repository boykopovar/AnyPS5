// Derived from Source/WTF/wtf/NumberOfCores.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2012 University of Szeged. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libkernel/include/Api.hpp"

#include <atomic>
#include <cstdint>

namespace {

constexpr int ScNprocessorsOnln = 58;
constexpr int DefaultIfUnavailable = 1;

using SysconfQuery = std::int64_t (*)(int);

std::int64_t GuestSysconf(int name) {
    return sysconf_nid_postfix(name);
}

std::atomic<SysconfQuery> g_sysconf{GuestSysconf};
std::atomic<int> g_numberOfCores{-1};

} // namespace

extern "C" void NumberOfProcessorCoresSysconfForTest_nid_no_patch(SysconfQuery query) {
    g_sysconf = query ? query : GuestSysconf;
    g_numberOfCores = -1;
}

namespace NKWebKit {

int NumberOfProcessorCores() {
    const int cached = g_numberOfCores;
    if (cached > 0) return cached;
    const std::int64_t result = g_sysconf.load()(ScNprocessorsOnln);
    const int numberOfCores = result < 0 ? DefaultIfUnavailable : static_cast<int>(result);
    g_numberOfCores = numberOfCores;
    return numberOfCores;
}

} // namespace NKWebKit
