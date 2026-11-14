// Derived from Source/WTF/wtf/posix/CPUTimePOSIX.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libkernel/Time/include/Time.hpp"

#include <atomic>
#include <stdexcept>

namespace {

constexpr int ClockThreadCputimeId = 14;

using ClockQuery = int (*)(int, KernelTimespec*);

int GuestClockGettime(int clockId, KernelTimespec* time) {
    return clock_gettime_nid_postfix(clockId, time);
}

std::atomic<ClockQuery> g_clock{GuestClockGettime};

double FromNanoseconds(double nanoseconds) {
    return nanoseconds / 1000 / 1000 / 1000;
}

} // namespace

extern "C" void CpuTimeClockForTest_nid_no_patch(ClockQuery query) {
    g_clock = query ? query : GuestClockGettime;
}

extern "C" {

WTF::Seconds APS5_VABI _ZN3WTF7CPUTime16forCurrentThreadEv() {
    KernelTimespec time{};
    if (g_clock.load()(ClockThreadCputimeId, &time) != 0)
        throw std::runtime_error("WTF::CPUTime::forCurrentThread: clock_gettime(CLOCK_THREAD_CPUTIME_ID) failed");
    return WTF::Seconds{static_cast<double>(time.tv_sec) + FromNanoseconds(static_cast<double>(time.tv_nsec))};
}

}
