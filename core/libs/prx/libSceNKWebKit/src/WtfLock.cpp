// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/LockAlgorithmInlines.h
/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/LockAlgorithm.h
/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/ParkingLot.cpp
/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WtfParking.hpp"

#include <atomic>
#include <chrono>
#include <random>
#include <stdexcept>
#include <thread>

namespace NKWebKit {

bool ParkingFairness_nid_no_patch() {
    static std::chrono::steady_clock::time_point nextFairTime;
    static std::minstd_rand random;
    const auto now = std::chrono::steady_clock::now();
    if (now <= nextFairTime) return false;
    const auto delay = std::chrono::duration<double, std::milli>(std::generate_canonical<double, 32>(random));
    nextFairTime = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(delay);
    return true;
}

}  // namespace NKWebKit

namespace {

using NKWebKit::WtfAbi::UnparkResult;

void LockSlow(std::uint8_t* self) {
    if (!self) throw std::runtime_error("WTF::ParkingLot: null address");
    std::atomic_ref byte(*self);
    unsigned spins = 0;
    for (;;) {
        auto value = byte.load();
        if (!(value & 1)) {
            if (byte.compare_exchange_weak(value, value | 1)) return;
            continue;
        }
        if (!(value & 2) && spins < 40) {
            ++spins;
            std::this_thread::yield();
            continue;
        }
        if (!(value & 2)) {
            if (!byte.compare_exchange_weak(value, value | 2)) continue;
            value |= 2;
        }
        const auto result = NKWebKit::ParkLock_nid_no_patch(self, value);
        if (result.wasUnparked && result.token == 1) return;
    }
}

void UnlockSlow(std::uint8_t* self, bool fair) {
    if (!self) throw std::runtime_error("WTF::ParkingLot: null address");
    std::atomic_ref byte(*self);
    for (;;) {
        auto value = byte.load();
        if (!(value & 1)) throw std::runtime_error("WTF::Lock: unlock of an unlocked lock");
        if (!(value & 2)) {
            if (byte.compare_exchange_weak(value, value & ~1)) return;
            continue;
        }
        struct Context { std::uint8_t* self; bool fair; } context{self, fair};
        NKWebKit::UnparkCallback_nid_no_patch(self, &context, [](void* data, UnparkResult result) -> std::intptr_t {
            const auto& context = *static_cast<Context*>(data);
            std::atomic_ref byte(*context.self);
            if (result.didUnparkThread && (context.fair || result.timeToBeFair)) return 1;
            auto current = byte.load();
            while (!byte.compare_exchange_weak(current, (current & ~3) | (result.mayHaveMoreThreads ? 2 : 0))) {}
            return 0;
        });
        return;
    }
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF4Lock8lockSlowEv(std::uint8_t* self) {
    LockSlow(self);
}

void APS5_VABI _ZN3WTF4Lock10unlockSlowEv(std::uint8_t* self) {
    UnlockSlow(self, false);
}

void APS5_VABI _ZN3WTF4Lock13safepointSlowEv(std::uint8_t* self) {
    UnlockSlow(self, true);
    LockSlow(self);
}

}
