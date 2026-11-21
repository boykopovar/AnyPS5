/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/PAL/pal/system/ClockGeneric.cpp
// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/PAL/pal/system/Clock.h
// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/PAL/pal/system/ClockGeneric.h

#include "prx/libSceNKWebKit/include/PalClockExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <new>
#include <stdexcept>

namespace {

using NKWebKit::PalAbi::Clock;
using NKWebKit::PalAbi::ClockVtable;

struct ErrnoGuard {
    int saved{errno};
    ~ErrnoGuard() { errno = saved; }
};

Clock ReadClock(const Clock* self) {
    ErrnoGuard guard;
    Clock value{};
    if (!GuestCopyIn_nid_no_patch(&value, self, sizeof(value)))
        throw std::invalid_argument("PAL::Clock: unreadable clock");
    return value;
}

void WriteClock(Clock* self, const Clock& value) {
    ErrnoGuard guard;
    if (!GuestCopyOut_nid_no_patch(self, &value, sizeof(value)))
        throw std::invalid_argument("PAL::Clock: unwritable clock");
}

double Now() {
    return _ZN3WTF13MonotonicTime3nowEv().m_value;
}

double CurrentTime(Clock& value) {
    if (value.running) value.lastTime = Now();
    return (value.lastTime - value.startTime) * value.rate + value.offset;
}

void APS5_VABI CompleteDestructor_nid_no_patch(Clock*) {
}

void APS5_VABI DeletingDestructor_nid_no_patch(Clock* self) {
    _ZN3WTF8fastFreeEPv(self);
}

void APS5_VABI SetCurrentTime_nid_no_patch(Clock* self, double time) {
    auto value = ReadClock(self);
    value.startTime = value.lastTime = Now();
    value.offset = time;
    WriteClock(self, value);
}

double APS5_VABI GetCurrentTime_nid_no_patch(const Clock* self) {
    auto value = ReadClock(self);
    const double result = CurrentTime(value);
    if (value.running) WriteClock(const_cast<Clock*>(self), value);
    return result;
}

void APS5_VABI SetPlayRate_nid_no_patch(Clock* self, double rate) {
    auto value = ReadClock(self);
    value.offset = CurrentTime(value);
    value.lastTime = value.startTime = Now();
    value.rate = rate;
    WriteClock(self, value);
}

double APS5_VABI GetPlayRate_nid_no_patch(const Clock* self) {
    return ReadClock(self).rate;
}

void APS5_VABI Start_nid_no_patch(Clock* self) {
    auto value = ReadClock(self);
    if (value.running) return;
    value.lastTime = value.startTime = Now();
    value.running = true;
    WriteClock(self, value);
}

void APS5_VABI Stop_nid_no_patch(Clock* self) {
    auto value = ReadClock(self);
    if (!value.running) return;
    value.offset = CurrentTime(value);
    value.lastTime = value.startTime = Now();
    value.running = false;
    WriteClock(self, value);
}

bool APS5_VABI IsRunning_nid_no_patch(const Clock* self) {
    return ReadClock(self).running != 0;
}

struct ClockVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    ClockVtable dispatch;
};

static_assert(offsetof(ClockVtableStorage, dispatch) == 16);

const ClockVtableStorage ClockDispatch{
    0,
    nullptr,
    {CompleteDestructor_nid_no_patch, DeletingDestructor_nid_no_patch, SetCurrentTime_nid_no_patch, GetCurrentTime_nid_no_patch, SetPlayRate_nid_no_patch, GetPlayRate_nid_no_patch, Start_nid_no_patch, Stop_nid_no_patch, IsRunning_nid_no_patch},
};

}  // namespace

NKWebKit::PalAbi::Clock** APS5_VABI _ZN3PAL5Clock6createEv(Clock** result) {
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(result, sizeof(*result)))
        throw std::invalid_argument("PAL::Clock::create: unwritable result");
    const double now = Now();
    auto* clock = new (_ZN3WTF10fastMallocEm(sizeof(Clock))) Clock{&ClockDispatch.dispatch, false, 1.0, 0.0, now, now};
    if (!GuestCopyOut_nid_no_patch(result, &clock, sizeof(clock))) {
        _ZN3WTF8fastFreeEPv(clock);
        throw std::invalid_argument("PAL::Clock::create: unwritable result");
    }
    return result;
}
