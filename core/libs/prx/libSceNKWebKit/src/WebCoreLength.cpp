// Derived from Source/WebCore/platform/Length.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

// Derived from Source/WebCore/platform/calc/CalculationValue.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreLengthExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace {

using NKWebKit::CalculationAbi::CalculationValue;
using NKWebKit::CalculationAbi::nextAvailableHandle;

struct CalculationValueMapEntry {
    std::uint64_t referenceCountMinusOne{0};
    CalculationValue* value{nullptr};
};

std::unordered_map<unsigned, CalculationValueMapEntry>& CalculationValues() {
    static std::unordered_map<unsigned, CalculationValueMapEntry> map;
    return map;
}

const WebCore::Length& Checked(const WebCore::Length* self) {
    if (!self) throw std::invalid_argument("null WebCore::Length");
    return *self;
}

WebCore::Length& Checked(WebCore::Length* self) {
    if (!self) throw std::invalid_argument("null WebCore::Length");
    return *self;
}

std::unordered_map<unsigned, CalculationValueMapEntry>::iterator Found(const WebCore::Length& length) {
    if (length.m_type != WebCore::LengthType::Calculated)
        throw std::invalid_argument("WebCore::Length: not calculated");
    auto& map = CalculationValues();
    auto it = map.find(length.m_calculationValueHandle);
    if (it == map.end())
        throw std::invalid_argument("WebCore::Length: unknown calculation value handle");
    return it;
}

using NodeEvaluateFunction = float (APS5_VABI*)(const void* node, float maxValue);
using NodeDestroyFunction = void (APS5_VABI*)(void* node);

float EvaluateNode(void* node, float maxValue) {
    void* const* vtable = reinterpret_cast<void* const*>(*reinterpret_cast<void**>(node));
    return reinterpret_cast<NodeEvaluateFunction>(const_cast<void*>(vtable[NKWebKit::CalculationAbi::NodeEvaluateSlot]))(node, maxValue);
}

void DestroyNode(void* node) {
    void* const* vtable = reinterpret_cast<void* const*>(*reinterpret_cast<void**>(node));
    reinterpret_cast<NodeDestroyFunction>(const_cast<void*>(vtable[NKWebKit::CalculationAbi::NodeDestroySlot]))(node);
}

void ConstructLength(WebCore::Length& self, WTF::Ref<CalculationValue>&& value) {
    CalculationValue* calculationValue = value.get();
    if (!calculationValue)
        throw std::invalid_argument("WebCore::Length: null CalculationValue");
    auto& map = CalculationValues();
    if (map.size() >= std::numeric_limits<unsigned>::max() - 1u)
        throw std::length_error("WebCore::Length: every CalculationValue handle is in use");
    unsigned handle = nextAvailableHandle;
    while (handle == 0 || handle == std::numeric_limits<unsigned>::max() || map.count(handle))
        ++handle;
    map.emplace(handle, CalculationValueMapEntry{0, calculationValue});
    value.leakRef();
    self.m_calculationValueHandle = handle;
    nextAvailableHandle = handle + 1;
    self.m_type = WebCore::LengthType::Calculated;
    self.m_hasQuirk = false;
    self.m_isFloat = false;
}

}  // namespace

namespace NKWebKit::CalculationAbi {

unsigned nextAvailableHandle{1};

void CalculationValue::ref() {
    ++m_refCount;
}

void CalculationValue::deref() {
    unsigned remaining = m_refCount - 1;
    if (remaining) {
        m_refCount = remaining;
        return;
    }
    if (m_expression)
        DestroyNode(m_expression);
    _ZN3WTF8fastFreeEPv(this);
}

}  // namespace NKWebKit::CalculationAbi

extern "C" {

void APS5_VABI _ZN7WebCore6LengthC1EON3WTF3RefINS_16CalculationValueENS1_13DumbPtrTraitsIS3_EEEE(WebCore::Length* self, WTF::Ref<CalculationValue>&& value) {
    ConstructLength(Checked(self), std::move(value));
}

void APS5_VABI _ZN7WebCore6LengthC2EON3WTF3RefINS_16CalculationValueENS1_13DumbPtrTraitsIS3_EEEE(WebCore::Length* self, WTF::Ref<CalculationValue>&& value) {
    ConstructLength(Checked(self), std::move(value));
}

void APS5_VABI _ZNK7WebCore6Length3refEv(const WebCore::Length* self) {
    ++Found(Checked(self))->second.referenceCountMinusOne;
}

void APS5_VABI _ZNK7WebCore6Length5derefEv(const WebCore::Length* self) {
    auto it = Found(Checked(self));
    if (it->second.referenceCountMinusOne) {
        --it->second.referenceCountMinusOne;
        return;
    }
    CalculationValue* value = it->second.value;
    CalculationValues().erase(it);
    value->deref();
}

float APS5_VABI _ZNK7WebCore6Length21nonNanCalculatedValueEi(const WebCore::Length* self, int maxValue) {
    CalculationValue& value = *Found(Checked(self))->second.value;
    if (!value.m_expression)
        throw std::invalid_argument("WebCore::Length: null calculation expression");
    float result = EvaluateNode(value.m_expression, static_cast<float>(maxValue));
    if (std::isnan(result))
        return 0;
    return value.m_shouldClampToNonNegative && result < 0 ? 0 : result;
}

}
