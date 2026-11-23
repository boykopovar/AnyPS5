// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/text/BidiContext.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All right reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/BidiContextExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::BidiContext;
using NKWebKit::WebCoreAbi::BidiContextRef;
using NKWebKit::WebCoreAbi::kBidiDirectionMask;
using NKWebKit::WebCoreAbi::kBidiDirectionShift;
using NKWebKit::WebCoreAbi::kBidiLevelMask;
using NKWebKit::WebCoreAbi::kBidiOverrideShift;
using NKWebKit::WebCoreAbi::kBidiSourceShift;

constexpr std::int32_t kLeftToRight = 0;
constexpr std::int32_t kRightToLeft = 1;
constexpr std::uint32_t kFromStyleOrDOM = 0;
constexpr std::uint32_t kFromUnicode = 1;

void Ref(BidiContext* context) {
    std::atomic_ref<unsigned>(context->refCount).fetch_add(1);
}

unsigned Level(const BidiContext& context) {
    return context.bits & kBidiLevelMask;
}

std::int32_t Direction(const BidiContext& context) {
    return static_cast<std::int32_t>((context.bits >> kBidiDirectionShift) & kBidiDirectionMask);
}

bool Override(const BidiContext& context) {
    return ((context.bits >> kBidiOverrideShift) & 1) != 0;
}

std::uint32_t Source(const BidiContext& context) {
    return (context.bits >> kBidiSourceShift) & 1;
}

BidiContext* CreateUncached(unsigned char level, std::int32_t direction, bool override, std::uint32_t source, BidiContext* parent) {
    auto* context = static_cast<BidiContext*>(_ZN3WTF10fastMallocEm(sizeof(BidiContext)));
    context->refCount = 1;
    context->bits = (level & kBidiLevelMask) | ((static_cast<std::uint32_t>(direction) & kBidiDirectionMask) << kBidiDirectionShift)
        | ((override ? 1U : 0U) << kBidiOverrideShift) | ((source & 1) << kBidiSourceShift);
    context->parent = parent;
    if (parent) Ref(parent);
    return context;
}

BidiContext* Create(unsigned char level, std::int32_t direction, bool override, std::uint32_t source, BidiContext* parent) {
    if (parent) return CreateUncached(level, direction, override, source, parent);
    BidiContext* shared = nullptr;
    if (!level) {
        if (!override) {
            static BidiContext* const ltrContext = CreateUncached(0, kLeftToRight, false, kFromStyleOrDOM, nullptr);
            shared = ltrContext;
        } else {
            static BidiContext* const ltrOverrideContext = CreateUncached(0, kLeftToRight, true, kFromStyleOrDOM, nullptr);
            shared = ltrOverrideContext;
        }
    } else if (!override) {
        static BidiContext* const rtlContext = CreateUncached(1, kRightToLeft, false, kFromStyleOrDOM, nullptr);
        shared = rtlContext;
    } else {
        static BidiContext* const rtlOverrideContext = CreateUncached(1, kRightToLeft, true, kFromStyleOrDOM, nullptr);
        shared = rtlOverrideContext;
    }
    Ref(shared);
    return shared;
}

BidiContext* CopyAndRebaselineLevel(const BidiContext& context, BidiContext* parent) {
    int newLevel = parent ? static_cast<int>(Level(*parent)) : 0;
    if (Direction(context) == kRightToLeft) newLevel = static_cast<unsigned char>((newLevel + 1) | 1);
    else if (parent) newLevel = static_cast<unsigned char>((newLevel + 2) & ~1);
    return Create(static_cast<unsigned char>(newLevel), Direction(context), Override(context), Source(context), parent);
}

}  // namespace

extern "C" {

BidiContextRef* APS5_VABI _ZN7WebCore11BidiContext6createEh14UCharDirectionbNS_19BidiEmbeddingSourceEPS0_(
    BidiContextRef* result, unsigned char level, std::int32_t direction, bool override, std::uint32_t source, BidiContext* parent) {
    if (!result) throw std::invalid_argument("WebCore::BidiContext::create: null result");
    result->ptr = Create(level, direction, override, source, parent);
    return result;
}

BidiContextRef* APS5_VABI _ZN7WebCore11BidiContext41copyStackRemovingUnicodeEmbeddingContextsEv(BidiContextRef* result, BidiContext* self) {
    if (!result) throw std::invalid_argument("WebCore::BidiContext::copyStackRemovingUnicodeEmbeddingContexts: null result");
    if (!self) throw std::invalid_argument("WebCore::BidiContext::copyStackRemovingUnicodeEmbeddingContexts: null this");
    std::vector<const BidiContext*> contexts;
    for (const BidiContext* ancestor = self; ancestor; ancestor = ancestor->parent)
        if (Source(*ancestor) != kFromUnicode) contexts.push_back(ancestor);
    if (contexts.empty()) throw std::invalid_argument("WebCore::BidiContext::copyStackRemovingUnicodeEmbeddingContexts: every context comes from Unicode");
    BidiContext* top = CopyAndRebaselineLevel(*contexts.back(), nullptr);
    for (std::size_t index = contexts.size() - 1; index; --index) {
        BidiContext* next = CopyAndRebaselineLevel(*contexts[index - 1], top);
        std::atomic_ref<unsigned>(top->refCount).fetch_sub(1);
        top = next;
    }
    result->ptr = top;
    return result;
}

}
