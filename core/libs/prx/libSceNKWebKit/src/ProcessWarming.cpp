// Derived from Source/WebCore/page/ProcessWarming.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

// Derived from Source/WebCore/platform/graphics/FontCache.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2006-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ProcessWarmingExports.hpp"

#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::PrewarmInformation;
using NKWebKit::WtfAbi::String;

PrewarmInformation* Checked(PrewarmInformation* self, const char* operation) {
    if (!self) throw std::invalid_argument(std::string("WebCore::PrewarmInformation::") + operation + ": null this");
    return self;
}

const PrewarmInformation& Source(const PrewarmInformation* other, const char* operation) {
    if (!other) throw std::invalid_argument(std::string("WebCore::PrewarmInformation::") + operation + ": null source");
    return *other;
}

void Validate(const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& vector, const char* operation) {
    if (vector.size > vector.capacity) throw std::invalid_argument(std::string("WebCore::PrewarmInformation::") + operation + ": vector size exceeds capacity");
    if (vector.size && !vector.buffer) throw std::invalid_argument(std::string("WebCore::PrewarmInformation::") + operation + ": null vector storage");
}

void Validate(const PrewarmInformation& information, const char* operation) {
    Validate(information.fontCache.seenFamilies, operation);
    Validate(information.fontCache.fontNamesRequiringSystemFallback, operation);
}

void DestroyVector(NKWebKit::WtfAbi::Vector<String>& vector) {
    for (unsigned i = 0; i < vector.size; ++i) vector.buffer[i].~String();
    if (vector.buffer) _ZN3WTF8fastFreeEPv(vector.buffer);
    vector.buffer = nullptr;
    vector.capacity = 0;
    vector.size = 0;
}

void ShrinkVector(NKWebKit::WtfAbi::Vector<String>& vector, unsigned size) {
    while (vector.size > size) vector.buffer[--vector.size].~String();
}

void ReserveVector(NKWebKit::WtfAbi::Vector<String>& vector, unsigned capacity) {
    if (capacity <= vector.capacity) return;
    String* oldBuffer = vector.buffer;
    const unsigned size = vector.size;
    vector.buffer = static_cast<String*>(_ZN3WTF10fastMallocEm(std::size_t{capacity} * sizeof(String)));
    vector.capacity = capacity;
    for (unsigned i = 0; i < size; ++i) {
        new (&vector.buffer[i]) String(std::move(oldBuffer[i]));
        oldBuffer[i].~String();
    }
    if (oldBuffer) _ZN3WTF8fastFreeEPv(oldBuffer);
}

void AssignVector(NKWebKit::WtfAbi::Vector<String>& target, const NKWebKit::WtfAbi::Vector<String>& source) {
    if (&target == &source) return;
    if (target.size > source.size) ShrinkVector(target, source.size);
    else if (source.size > target.capacity) {
        DestroyVector(target);
        ReserveVector(target, source.size);
    }
    unsigned index = 0;
    for (; index < target.size; ++index) target.buffer[index] = source.buffer[index];
    for (; index < source.size; ++index) new (&target.buffer[index]) String(source.buffer[index]);
    target.size = source.size;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore18PrewarmInformationC1Ev(PrewarmInformation* self) {
    new (Checked(self, "PrewarmInformation")) PrewarmInformation();
}

void APS5_VABI _ZN7WebCore18PrewarmInformationC2Ev(PrewarmInformation* self) {
    new (Checked(self, "PrewarmInformation")) PrewarmInformation();
}

void APS5_VABI _ZN7WebCore18PrewarmInformationC1ERKS0_(PrewarmInformation* self, const PrewarmInformation* other) {
    const PrewarmInformation& source = Source(other, "PrewarmInformation");
    Validate(source, "PrewarmInformation");
    new (Checked(self, "PrewarmInformation")) PrewarmInformation(source);
}

void APS5_VABI _ZN7WebCore18PrewarmInformationC2ERKS0_(PrewarmInformation* self, const PrewarmInformation* other) {
    const PrewarmInformation& source = Source(other, "PrewarmInformation");
    Validate(source, "PrewarmInformation");
    new (Checked(self, "PrewarmInformation")) PrewarmInformation(source);
}

PrewarmInformation& APS5_VABI _ZN7WebCore18PrewarmInformationaSERKS0_(PrewarmInformation* self, const PrewarmInformation* other) {
    PrewarmInformation* target = Checked(self, "operator=");
    const PrewarmInformation& source = Source(other, "operator=");
    if (target != &source) {
        Validate(*target, "operator=");
        Validate(source, "operator=");
        AssignVector(target->fontCache.seenFamilies, source.fontCache.seenFamilies);
        AssignVector(target->fontCache.fontNamesRequiringSystemFallback, source.fontCache.fontNamesRequiringSystemFallback);
    }
    return *target;
}

void APS5_VABI _ZN7WebCore18PrewarmInformationD1Ev(PrewarmInformation* self) {
    PrewarmInformation* target = Checked(self, "~PrewarmInformation");
    Validate(*target, "~PrewarmInformation");
    DestroyVector(target->fontCache.seenFamilies);
    DestroyVector(target->fontCache.fontNamesRequiringSystemFallback);
}

void APS5_VABI _ZN7WebCore18PrewarmInformationD2Ev(PrewarmInformation* self) {
    PrewarmInformation* target = Checked(self, "~PrewarmInformation");
    Validate(*target, "~PrewarmInformation");
    DestroyVector(target->fontCache.seenFamilies);
    DestroyVector(target->fontCache.fontNamesRequiringSystemFallback);
}

PrewarmInformation APS5_VABI _ZN7WebCore14ProcessWarming25collectPrewarmInformationEv() {
    return PrewarmInformation();
}

void APS5_VABI _ZN7WebCore14ProcessWarming22prewarmWithInformationERKNS_18PrewarmInformationE(const PrewarmInformation* prewarmInformation) {
    if (!prewarmInformation) throw std::invalid_argument("WebCore::ProcessWarming::prewarmWithInformation: null prewarm information");
}

}
