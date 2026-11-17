/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * Derived from Source/WTF/wtf/ObjectIdentifier.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2013 Andrew Bortz. All rights reserved.
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
 * Derived from Source/WTF/wtf/RandomNumber.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2009, 2015 Apple Inc. All rights reserved.
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
 *
 * Vigna, Sebastiano (2014). "Further scramblings of Marsaglia's xorshift
 * generators". arXiv:1404.0390 (http://arxiv.org/abs/1404.0390)
 *
 * See also https://en.wikipedia.org/wiki/Xorshift.
 * Derived from Source/WTF/wtf/WeakRandom.h@c55364d53f971f63a709976f168d932d558b8909
 */
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include "prx/libSceJsc/include/WtfRandomExports.hpp"

#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>

namespace {

class WeakRandom {
public:
    explicit WeakRandom(unsigned int seed) {
        if (!seed) seed = 1;
        m_low = seed;
        m_high = seed;
        Advance();
    }

    unsigned int GetUint32() {
        return static_cast<unsigned int>(Advance());
    }

private:
    static std::uint64_t NextState(std::uint64_t x, std::uint64_t y) {
        x ^= x << 23;
        x ^= x >> 17;
        x ^= y ^ (y >> 26);
        return x;
    }

    std::uint64_t Advance() {
        const std::uint64_t x = m_low;
        const std::uint64_t y = m_high;
        m_low = y;
        m_high = NextState(x, y);
        return m_high + m_low;
    }

    std::uint64_t m_low = 0;
    std::uint64_t m_high = 0;
};

std::atomic<std::uint64_t> g_identifier{0};
std::atomic<std::uint64_t> g_threadSafeIdentifier{0};

}  // namespace

extern "C" {

std::uint32_t APS5_VABI _ZN3WTF29cryptographicallyRandomNumberEv() {
    std::uint32_t value = 0;
    HostEntropy_nid_no_patch(&value, sizeof(value));
    return value;
}

double APS5_VABI _ZN3WTF12randomNumberEv() {
    const std::uint32_t bits = _ZN3WTF29cryptographicallyRandomNumberEv();
    return static_cast<double>(bits) / (static_cast<double>(std::numeric_limits<std::uint32_t>::max()) + 1.0);
}

unsigned int APS5_VABI _ZN3WTF16weakRandomUint32Ev() {
    static std::mutex mutex;
    static WeakRandom random(_ZN3WTF29cryptographicallyRandomNumberEv());
    std::lock_guard lock(mutex);
    return random.GetUint32();
}

std::uint64_t APS5_VABI _ZN3WTF20ObjectIdentifierBase26generateIdentifierInternalEv() {
    return ++g_identifier;
}

std::uint64_t APS5_VABI _ZN3WTF20ObjectIdentifierBase36generateThreadSafeIdentifierInternalEv() {
    return ++g_threadSafeIdentifier;
}

}
