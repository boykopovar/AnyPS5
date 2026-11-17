// Derived from Source/WTF/wtf/CurrentTime.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Google Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libkernel/Time/include/Time.hpp"

#include <atomic>

namespace {

constexpr int ClockRealtime = 0;
constexpr int ClockMonotonic = 4;

using ClockQuery = int (*)(int, KernelTimespec*);

int GuestClockGettime(int clockId, KernelTimespec* time) {
    return clock_gettime_nid_postfix(clockId, time);
}

std::atomic<ClockQuery> g_clock{GuestClockGettime};

double ClockSeconds(int clockId) {
    KernelTimespec time{};
    g_clock.load()(clockId, &time);
    return static_cast<double>(time.tv_sec) + time.tv_nsec / 1'000'000'000.0;
}

} // namespace

extern "C" void CurrentTimeClockForTest_nid_no_patch(ClockQuery query) {
    g_clock = query ? query : GuestClockGettime;
}

namespace NKWebKit {

double CurrentWallTime() {
    return ClockSeconds(ClockRealtime);
}

double CurrentMonotonicTime() {
    return ClockSeconds(ClockMonotonic);
}

} // namespace NKWebKit
