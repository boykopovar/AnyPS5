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

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/BidiContextExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::BidiContext;
using NKWebKit::WebCoreAbi::BidiContextRef;
using NKWebKit::WebCoreAbi::kBidiDirectionMask;
using NKWebKit::WebCoreAbi::kBidiDirectionShift;
using NKWebKit::WebCoreAbi::kBidiLevelMask;
using NKWebKit::WebCoreAbi::kBidiOverrideShift;
using NKWebKit::WebCoreAbi::kBidiSourceShift;

static_assert(std::is_trivially_copyable_v<BidiContextRef>);

constexpr std::int32_t kLeftToRight = 0;
constexpr std::int32_t kRightToLeft = 1;
constexpr std::uint32_t kFromStyleOrDOM = 0;
constexpr std::uint32_t kFromUnicode = 1;

template<typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void RequireWritableGuestBytes(void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, size);
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, size);
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void Ref(BidiContext* context, const char* function) {
    unsigned count = 0;
    auto* const field = reinterpret_cast<unsigned char*>(context) + offsetof(BidiContext, refCount);
    if (!GuestCopyIn_nid_no_patch(&count, field, sizeof(count)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    ++count;
    if (!GuestCopyOut_nid_no_patch(field, &count, sizeof(count)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RefShared(BidiContext* shared) {
    std::atomic_ref<unsigned>(shared->refCount).fetch_add(1);
}

void DecrementRefCountNoThrow(BidiContext* context, bool isShared = false) noexcept {
    if (!context) return;
    try {
        if (isShared) {
            std::atomic_ref<unsigned>(context->refCount).fetch_sub(1);
            return;
        }
        unsigned count = 0;
        auto* const field = reinterpret_cast<unsigned char*>(context) + offsetof(BidiContext, refCount);
        if (!GuestCopyIn_nid_no_patch(&count, field, sizeof(count)) || !count) return;
        --count;
        static_cast<void>(GuestCopyOut_nid_no_patch(field, &count, sizeof(count)));
    } catch (...) {
    }
}

struct RawAllocGuard {
    BidiContext* ptr = nullptr;
    BidiContext* referencedParent = nullptr;

    ~RawAllocGuard() {
        if (!ptr) return;
        DecrementRefCountNoThrow(referencedParent);
        try {
            _ZN3WTF8fastFreeEPv(ptr);
        } catch (...) {
        }
    }
};

struct CreatedContext {
    BidiContext* ptr = nullptr;
    BidiContext* referencedParent = nullptr;
    bool allocated = false;
    bool ownsLocalRef = true;
};

struct CreatedContextGuard {
    CreatedContext created{};

    ~CreatedContextGuard() {
        if (!created.ptr) return;
        try {
            if (created.allocated) {
                DecrementRefCountNoThrow(created.referencedParent);
                _ZN3WTF8fastFreeEPv(created.ptr);
            } else if (created.ownsLocalRef) {
                DecrementRefCountNoThrow(created.ptr, true);
            }
        } catch (...) {
        }
    }
};

struct CopiedStackGuard {
    std::vector<CreatedContext> nodes;

    void PushAndTransferParentRef(CreatedContext node) {
        const std::size_t previousCount = nodes.size();
        nodes.push_back(node);
        if (previousCount) {
            auto& previous = nodes[previousCount - 1];
            DecrementRefCountNoThrow(previous.ptr, !previous.allocated);
            previous.ownsLocalRef = false;
        }
    }

    BidiContext* Top() const {
        return nodes.empty() ? nullptr : nodes.back().ptr;
    }

    void ReleaseOwnership() noexcept {
        nodes.clear();
    }

    ~CopiedStackGuard() {
        for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
            try {
                if (it->allocated) {
                    DecrementRefCountNoThrow(it->referencedParent);
                    _ZN3WTF8fastFreeEPv(it->ptr);
                } else if (it->ownsLocalRef) {
                    DecrementRefCountNoThrow(it->ptr, true);
                }
            } catch (...) {
            }
        }
    }
};

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

BidiContext* CreateUncached(unsigned char level, std::int32_t direction, bool override, std::uint32_t source, BidiContext* parent, const char* function) {
    if (parent) {
        static_cast<void>(ReadGuestValue(parent, function));
        RequireWritableGuestBytes(parent, sizeof(BidiContext), function);
    }
    auto* context = static_cast<BidiContext*>(_ZN3WTF10fastMallocEm(sizeof(BidiContext)));
    RawAllocGuard alloc{context};
    context->refCount = 1;
    context->bits = (level & kBidiLevelMask) | ((static_cast<std::uint32_t>(direction) & kBidiDirectionMask) << kBidiDirectionShift)
        | ((override ? 1U : 0U) << kBidiOverrideShift) | ((source & 1) << kBidiSourceShift);
    context->parent = parent;
    if (parent) {
        Ref(parent, function);
        alloc.referencedParent = parent;
    }
    alloc.ptr = nullptr;
    alloc.referencedParent = nullptr;
    return context;
}

CreatedContext Create(unsigned char level, std::int32_t direction, bool override, std::uint32_t source, BidiContext* parent, const char* function) {
    if (parent) return CreatedContext{CreateUncached(level, direction, override, source, parent, function), parent, true, true};
    BidiContext* shared = nullptr;
    if (!level) {
        if (!override) {
            static BidiContext* const ltrContext = CreateUncached(0, kLeftToRight, false, kFromStyleOrDOM, nullptr, function);
            shared = ltrContext;
        } else {
            static BidiContext* const ltrOverrideContext = CreateUncached(0, kLeftToRight, true, kFromStyleOrDOM, nullptr, function);
            shared = ltrOverrideContext;
        }
    } else if (!override) {
        static BidiContext* const rtlContext = CreateUncached(1, kRightToLeft, false, kFromStyleOrDOM, nullptr, function);
        shared = rtlContext;
    } else {
        static BidiContext* const rtlOverrideContext = CreateUncached(1, kRightToLeft, true, kFromStyleOrDOM, nullptr, function);
        shared = rtlOverrideContext;
    }
    RefShared(shared);
    return CreatedContext{shared, nullptr, false, true};
}

CreatedContext CopyAndRebaselineLevel(const BidiContext& context, BidiContext* parent, const char* function) {
    int newLevel = parent ? static_cast<int>(Level(ReadGuestValue(parent, function))) : 0;
    if (Direction(context) == kRightToLeft) newLevel = static_cast<unsigned char>((newLevel + 1) | 1);
    else if (parent) newLevel = static_cast<unsigned char>((newLevel + 2) & ~1);
    return Create(static_cast<unsigned char>(newLevel), Direction(context), Override(context), Source(context), parent, function);
}

}  // namespace

extern "C" {

BidiContextRef* APS5_VABI _ZN7WebCore11BidiContext6createEh14UCharDirectionbNS_19BidiEmbeddingSourceEPS0_(
    BidiContextRef* result, unsigned char level, std::int32_t direction, bool override, std::uint32_t source, BidiContext* parent) {
    if (!result) throw std::invalid_argument("WebCore::BidiContext::create: null result");
    RequireWritableGuestBytes(result, sizeof(BidiContextRef), __func__);
    CreatedContextGuard created{Create(level, direction, override, source, parent, __func__)};
    const BidiContextRef output{created.created.ptr};
    if (!GuestCopyOut_nid_no_patch(result, &output, sizeof(output)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    created.created.ptr = nullptr;
    return result;
}

BidiContextRef* APS5_VABI _ZN7WebCore11BidiContext41copyStackRemovingUnicodeEmbeddingContextsEv(BidiContextRef* result, BidiContext* self) {
    if (!result) throw std::invalid_argument("WebCore::BidiContext::copyStackRemovingUnicodeEmbeddingContexts: null result");
    if (!self) throw std::invalid_argument("WebCore::BidiContext::copyStackRemovingUnicodeEmbeddingContexts: null this");
    RequireWritableGuestBytes(result, sizeof(BidiContextRef), __func__);
    std::vector<BidiContext> contexts;
    std::unordered_set<const BidiContext*> visited;
    for (const BidiContext* ancestor = self; ancestor;) {
        if (!visited.insert(ancestor).second)
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        const BidiContext snapshot = ReadGuestValue(ancestor, __func__);
        if (Source(snapshot) != kFromUnicode) contexts.push_back(snapshot);
        ancestor = snapshot.parent;
    }
    if (contexts.empty()) throw std::invalid_argument("WebCore::BidiContext::copyStackRemovingUnicodeEmbeddingContexts: every context comes from Unicode");
    CopiedStackGuard stack;
    stack.nodes.reserve(contexts.size());
    {
        CreatedContextGuard root{CopyAndRebaselineLevel(contexts.back(), nullptr, __func__)};
        stack.PushAndTransferParentRef(root.created);
        root.created.ptr = nullptr;
    }
    for (std::size_t index = contexts.size() - 1; index; --index) {
        CreatedContextGuard next{CopyAndRebaselineLevel(contexts[index - 1], stack.Top(), __func__)};
        stack.PushAndTransferParentRef(next.created);
        next.created.ptr = nullptr;
    }
    const BidiContextRef output{stack.Top()};
    if (!GuestCopyOut_nid_no_patch(result, &output, sizeof(output)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    stack.ReleaseOwnership();
    return result;
}

}
