/*
 * Copyright (C) 2005, 2006, 2008, 2011, 2014 Apple Inc. All rights reserved.
 *
 * Derived from Source/WebCore/history/HistoryItem.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/include/HistoryItemExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::CachedPage;
using NKWebKit::WebCoreAbi::HistoryItem;
using NKWebKit::WebCoreAbi::HistoryItemVectorStorage;
using NKWebKit::WebCoreAbi::historyItemChangedHook;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using WebCore::IntPoint;

constexpr unsigned kStringImplFlag8Bit = 4u;
constexpr unsigned kStringImplBufferOwnershipMask = 3u;
constexpr unsigned kStringImplBufferSubstring = 2u;
constexpr unsigned kStringImplBufferOwned = 3u;
constexpr unsigned kStringImplTerminatingFlagsMask = 48u;
constexpr std::size_t kSubstringHeaderBytes = 24u + sizeof(StringImpl*);

struct PlannedStringState {
    StringImpl* impl{nullptr};
    unsigned originalRefCount{0};
    unsigned refCount{0};
    unsigned hashAndFlags{0};
    bool destroyed{false};
};

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template <typename T>
T ReadGuestValue(const void* pointer, const char* function) {
    RequireReadableGuestBytes(pointer, sizeof(T), function);
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template <typename T>
T ReadGuestField(const void* base, std::size_t offset, const char* function) {
    return ReadGuestValue<T>(reinterpret_cast<const unsigned char*>(base) + offset, function);
}

template <typename T>
void WriteRawField(std::array<unsigned char, sizeof(HistoryItem)>& raw, std::size_t offset, const T& value) {
    std::memcpy(raw.data() + offset, &value, sizeof(T));
}

const HistoryItem* RequireReadableHistoryItem(const HistoryItem* self, const char* function) {
    if (!self) throw std::invalid_argument("null WebCore::HistoryItem");
    RequireReadableGuestBytes(self, sizeof(HistoryItem), function);
    return self;
}

HistoryItem* RequireNonNullHistoryItem(HistoryItem* self) {
    if (!self) throw std::invalid_argument("null WebCore::HistoryItem");
    return self;
}

struct WritableHistoryItem {
    HistoryItem* item{nullptr};
    GuestArena::HostWrite writeGuard;

    WritableHistoryItem(HistoryItem* self, const char* function, bool requireReadable)
        : item(RequireNonNullHistoryItem(self)), writeGuard(item, sizeof(HistoryItem)) {
        if (requireReadable) RequireReadableGuestBytes(item, sizeof(HistoryItem), function);
        if (!writeGuard.Open() || !GuestWritable_nid_no_patch(item, sizeof(HistoryItem)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
};

void ValidateReadableStringImpl(const StringImpl* impl, const char* function) {
    if (!impl) return;
    const StringImpl header = ReadGuestValue<StringImpl>(impl, function);
    if (!header.length) return;
    if (!header.data) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t unitSize = (header.hashAndFlags & kStringImplFlag8Bit) != 0 ? 1u : sizeof(std::uint16_t);
    if (header.length > std::numeric_limits<std::size_t>::max() / unitSize)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    RequireReadableGuestBytes(header.data, static_cast<std::size_t>(header.length) * unitSize, function);
}

std::size_t EnsurePlannedString(StringImpl* impl, const char* function, std::vector<PlannedStringState>& planned, std::deque<GuestArena::HostWrite>& writeGuards) {
    for (std::size_t i = 0; i < planned.size(); ++i) {
        if (planned[i].impl == impl) return i;
    }
    const StringImpl header = ReadGuestValue<StringImpl>(impl, function);
    auto& guard = writeGuards.emplace_back(impl, sizeof(StringImpl));
    if (!guard.Open() || !GuestWritable_nid_no_patch(impl, sizeof(StringImpl)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    planned.push_back({impl, header.refCount, header.refCount, header.hashAndFlags, false});
    return planned.size() - 1;
}

void PlanRetain(StringImpl* impl, const char* function, std::vector<PlannedStringState>& planned, std::deque<GuestArena::HostWrite>& writeGuards) {
    if (!impl) return;
    const std::size_t index = EnsurePlannedString(impl, function, planned, writeGuards);
    if (planned[index].destroyed || planned[index].refCount > std::numeric_limits<unsigned>::max() - 2u)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    planned[index].refCount += 2u;
}

void PlanRelease(StringImpl* impl, const char* function, std::vector<PlannedStringState>& planned, std::deque<GuestArena::HostWrite>& writeGuards, unsigned depth = 0) {
    if (!impl) return;
    if (depth >= 64u) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t index = EnsurePlannedString(impl, function, planned, writeGuards);
    if (planned[index].destroyed) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (planned[index].refCount != 2u) {
        planned[index].refCount -= 2u;
        return;
    }
    const unsigned flags = planned[index].hashAndFlags;
    if ((flags & kStringImplTerminatingFlagsMask) != 0u || (flags & kStringImplBufferOwnershipMask) == kStringImplBufferOwned)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    planned[index].destroyed = true;
    if ((flags & kStringImplBufferOwnershipMask) == kStringImplBufferSubstring) {
        RequireReadableGuestBytes(impl, kSubstringHeaderBytes, function);
        auto* owner = ReadGuestField<StringImpl*>(impl, 24, function);
        if (owner) PlanRelease(owner, function, planned, writeGuards, depth + 1u);
    }
}

void RollbackRetains(const std::vector<std::pair<StringImpl*, unsigned>>& retained) {
    for (std::size_t i = retained.size(); i > 0; --i) {
        const auto& [impl, originalRefCount] = retained[i - 1];
        GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(impl) + offsetof(StringImpl, refCount), &originalRefCount, sizeof(originalRefCount));
    }
}

void ApplyRetains(const std::vector<StringImpl*>& toRetain, const std::vector<PlannedStringState>& planned, std::vector<std::pair<StringImpl*, unsigned>>& retained, const char* function) {
    std::vector<std::pair<StringImpl*, unsigned>> counts;
    for (StringImpl* impl : toRetain) {
        if (!impl) continue;
        bool found = false;
        for (auto& entry : counts) {
            if (entry.first == impl) {
                entry.second += 2u;
                found = true;
                break;
            }
        }
        if (!found) counts.emplace_back(impl, 2u);
    }
    retained.reserve(counts.size());
    for (const auto& [impl, delta] : counts) {
        unsigned originalRefCount = 0;
        for (const auto& state : planned) {
            if (state.impl == impl) {
                originalRefCount = state.originalRefCount;
                break;
            }
        }
        const unsigned updatedRefCount = originalRefCount + delta;
        retained.emplace_back(impl, originalRefCount);
        if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(impl) + offsetof(StringImpl, refCount), &updatedRefCount, sizeof(updatedRefCount))) {
            retained.pop_back();
            RollbackRetains(retained);
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
    }
}

void AssignFieldString(HistoryItem* self, std::size_t fieldOffset, const String* value, bool notify, const char* function) {
    RequireNonNullHistoryItem(self);
    if (!value) throw std::invalid_argument("null WTF::String");
    const WritableHistoryItem writable(self, function, true);
    auto* const sourceImpl = ReadGuestValue<StringImpl*>(value, function);
    auto* const targetImpl = ReadGuestField<StringImpl*>(self, fieldOffset, function);
    if (targetImpl != sourceImpl) {
        std::vector<PlannedStringState> planned;
        std::deque<GuestArena::HostWrite> stringWrites;
        PlanRetain(sourceImpl, function, planned, stringWrites);
        PlanRelease(targetImpl, function, planned, stringWrites);
        std::vector<std::pair<StringImpl*, unsigned>> retained;
        ApplyRetains({sourceImpl}, planned, retained, function);
        if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(self) + fieldOffset, &sourceImpl, sizeof(sourceImpl))) {
            RollbackRetains(retained);
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
        NKWebKit::WtfAbi::Release(targetImpl);
    }
    if (notify) historyItemChangedHook(self);
}

void HistoryItemChangedNoOp(HistoryItem*) {
}

std::once_flag sequenceNumberOnce;
std::int64_t sequenceNumberNext = 0;

std::int64_t GenerateSequenceNumber() {
    std::call_once(sequenceNumberOnce, [] {
        sequenceNumberNext = static_cast<std::int64_t>(_ZN3WTF8WallTime3nowEv().m_value * 1000.0 * 1000.0);
    });
    return ++sequenceNumberNext;
}

void Construct(HistoryItem* self, const String* urlString, const String* title, const String* alternateTitle, const NKWebKit::WebCoreAbi::BackForwardItemIdentifier& identifier, const char* function) {
    if (!self) throw std::invalid_argument("null WebCore::HistoryItem");
    if (!urlString) throw std::invalid_argument("null WTF::String");
    if (!title) throw std::invalid_argument("null WTF::String");
    if (!alternateTitle) throw std::invalid_argument("null WTF::String");
    const WritableHistoryItem writable(self, function, false);
    auto* const urlImpl = ReadGuestValue<StringImpl*>(urlString, function);
    auto* const titleImpl = ReadGuestValue<StringImpl*>(title, function);
    auto* const altImpl = ReadGuestValue<StringImpl*>(alternateTitle, function);

    std::vector<PlannedStringState> planned;
    std::deque<GuestArena::HostWrite> stringWrites;
    PlanRetain(urlImpl, function, planned, stringWrites);
    PlanRetain(urlImpl, function, planned, stringWrites);
    PlanRetain(titleImpl, function, planned, stringWrites);
    PlanRetain(altImpl, function, planned, stringWrites);

    std::array<unsigned char, sizeof(HistoryItem)> raw{};
    const unsigned refCount = 1u;
    const bool restoreScroll = true;
    const std::int64_t itemSeq = GenerateSequenceNumber();
    const std::int64_t docSeq = GenerateSequenceNumber();
    WriteRawField(raw, offsetof(HistoryItem, refCount), refCount);
    WriteRawField(raw, offsetof(HistoryItem, urlString), urlImpl);
    WriteRawField(raw, offsetof(HistoryItem, originalURLString), urlImpl);
    WriteRawField(raw, offsetof(HistoryItem, title), titleImpl);
    WriteRawField(raw, offsetof(HistoryItem, displayTitle), altImpl);
    WriteRawField(raw, offsetof(HistoryItem, shouldRestoreScrollPosition), restoreScroll);
    WriteRawField(raw, offsetof(HistoryItem, itemSequenceNumber), itemSeq);
    WriteRawField(raw, offsetof(HistoryItem, documentSequenceNumber), docSeq);
    WriteRawField(raw, offsetof(HistoryItem, identifier), identifier);

    std::vector<std::pair<StringImpl*, unsigned>> retained;
    ApplyRetains({urlImpl, urlImpl, titleImpl, altImpl}, planned, retained, function);
    if (!GuestCopyOut_nid_no_patch(self, raw.data(), raw.size())) {
        RollbackRetains(retained);
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
}

void AssignDocumentState(HistoryItem* self, const HistoryItemVectorStorage* state, const char* function) {
    RequireNonNullHistoryItem(self);
    if (!state) throw std::invalid_argument("null WTF::Vector");
    const WritableHistoryItem writable(self, function, true);
    if (reinterpret_cast<const unsigned char*>(state) == reinterpret_cast<const unsigned char*>(self) + offsetof(HistoryItem, documentState))
        return;

    const auto source = ReadGuestValue<HistoryItemVectorStorage>(state, function);
    const auto target = ReadGuestField<HistoryItemVectorStorage>(self, offsetof(HistoryItem, documentState), function);
    if (source.size > source.capacity || target.size > target.capacity)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (source.size > 0 && !source.buffer)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if ((target.size > 0 || target.capacity > 0) && !target.buffer)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (source.size > std::numeric_limits<std::size_t>::max() / sizeof(String) ||
        target.capacity > std::numeric_limits<std::size_t>::max() / sizeof(String))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);

    std::vector<StringImpl*> sourceImpls;
    if (source.size > 0) {
        const std::size_t sourceBytes = static_cast<std::size_t>(source.size) * sizeof(StringImpl*);
        RequireReadableGuestBytes(source.buffer, sourceBytes, function);
        sourceImpls.resize(source.size, nullptr);
        if (!GuestCopyIn_nid_no_patch(sourceImpls.data(), source.buffer, sourceBytes))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }

    std::vector<StringImpl*> targetImpls;
    if (target.size > 0) {
        const std::size_t targetBytes = static_cast<std::size_t>(target.size) * sizeof(StringImpl*);
        RequireReadableGuestBytes(target.buffer, targetBytes, function);
        targetImpls.resize(target.size, nullptr);
        if (!GuestCopyIn_nid_no_patch(targetImpls.data(), target.buffer, targetBytes))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }

    std::vector<PlannedStringState> planned;
    std::deque<GuestArena::HostWrite> stringWrites;
    for (StringImpl* impl : sourceImpls) PlanRetain(impl, function, planned, stringWrites);
    for (StringImpl* impl : targetImpls) PlanRelease(impl, function, planned, stringWrites);

    const bool reallocate = source.size > target.capacity;
    if (reallocate) {
        void* const newBuffer = _ZN3WTF10fastMallocEm(static_cast<std::size_t>(source.size) * sizeof(StringImpl*));
        if (!newBuffer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        std::memcpy(newBuffer, sourceImpls.data(), static_cast<std::size_t>(source.size) * sizeof(StringImpl*));
        std::vector<std::pair<StringImpl*, unsigned>> retained;
        try {
            ApplyRetains(sourceImpls, planned, retained, function);
        } catch (...) {
            _ZN3WTF8fastFreeEPv(newBuffer);
            throw;
        }
        const HistoryItemVectorStorage updated{newBuffer, source.size, source.size};
        if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(self) + offsetof(HistoryItem, documentState), &updated, sizeof(updated))) {
            RollbackRetains(retained);
            _ZN3WTF8fastFreeEPv(newBuffer);
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
        for (StringImpl* impl : targetImpls) NKWebKit::WtfAbi::Release(impl);
        if (target.buffer) _ZN3WTF8fastFreeEPv(target.buffer);
        return;
    }

    const std::size_t writeBytes = static_cast<std::size_t>(source.size) * sizeof(StringImpl*);
    std::deque<GuestArena::HostWrite> bufferWrite;
    if (writeBytes > 0) {
        auto& guard = bufferWrite.emplace_back(target.buffer, writeBytes);
        if (!guard.Open() || !GuestWritable_nid_no_patch(target.buffer, writeBytes))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    std::vector<std::pair<StringImpl*, unsigned>> retained;
    ApplyRetains(sourceImpls, planned, retained, function);
    if (writeBytes > 0 && !GuestCopyOut_nid_no_patch(target.buffer, sourceImpls.data(), writeBytes)) {
        RollbackRetains(retained);
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    const HistoryItemVectorStorage updated{target.buffer, target.capacity, source.size};
    if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(self) + offsetof(HistoryItem, documentState), &updated, sizeof(updated))) {
        if (target.size > 0) {
            const std::size_t restoreCount = std::min(target.size, source.size);
            if (restoreCount > 0)
                GuestCopyOut_nid_no_patch(target.buffer, targetImpls.data(), static_cast<std::size_t>(restoreCount) * sizeof(StringImpl*));
        }
        RollbackRetains(retained);
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    for (StringImpl* impl : targetImpls) NKWebKit::WtfAbi::Release(impl);
}

void AppendChild(HistoryItem* self, HistoryItem** refSlot, const char* function) {
    RequireNonNullHistoryItem(self);
    if (!refSlot) throw std::invalid_argument("null WTF::Ref");
    const WritableHistoryItem writable(self, function, true);
    RequireReadableGuestBytes(refSlot, sizeof(HistoryItem*), function);
    const GuestArena::HostWrite slotWrite(refSlot, sizeof(HistoryItem*));
    if (!slotWrite.Open() || !GuestWritable_nid_no_patch(refSlot, sizeof(HistoryItem*)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);

    auto* const childPtr = ReadGuestValue<HistoryItem*>(refSlot, function);
    if (childPtr) RequireReadableGuestBytes(childPtr, sizeof(HistoryItem), function);
    const auto children = ReadGuestField<HistoryItemVectorStorage>(self, offsetof(HistoryItem, children), function);
    if (children.size > children.capacity || children.size == std::numeric_limits<unsigned>::max())
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if ((children.size > 0 || children.capacity > 0) && !children.buffer)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);

    HistoryItem* const nullChild = nullptr;
    if (children.size == children.capacity) {
        const std::size_t newMinCapacity = static_cast<std::size_t>(children.size) + 1;
        const std::size_t growth = static_cast<std::size_t>(children.capacity) + children.capacity / 4 + 1;
        const std::size_t newCapacity = std::max(newMinCapacity, std::max<std::size_t>(16, growth));
        if (newCapacity > std::numeric_limits<unsigned>::max() ||
            newCapacity > std::numeric_limits<std::size_t>::max() / sizeof(HistoryItem*))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        const std::size_t existingBytes = static_cast<std::size_t>(children.size) * sizeof(HistoryItem*);
        if (existingBytes > 0) RequireReadableGuestBytes(children.buffer, existingBytes, function);
        auto* const newBuffer = static_cast<HistoryItem**>(_ZN3WTF10fastMallocEm(newCapacity * sizeof(HistoryItem*)));
        if (!newBuffer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        if (existingBytes > 0 && !GuestCopyIn_nid_no_patch(newBuffer, children.buffer, existingBytes)) {
            _ZN3WTF8fastFreeEPv(newBuffer);
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
        newBuffer[children.size] = childPtr;
        const HistoryItemVectorStorage updated{newBuffer, static_cast<unsigned>(newCapacity), children.size + 1};
        if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(self) + offsetof(HistoryItem, children), &updated, sizeof(updated))) {
            _ZN3WTF8fastFreeEPv(newBuffer);
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
        if (!GuestCopyOut_nid_no_patch(refSlot, &nullChild, sizeof(nullChild))) {
            if (GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(self) + offsetof(HistoryItem, children), &children, sizeof(children)))
                _ZN3WTF8fastFreeEPv(newBuffer);
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
        if (children.buffer) _ZN3WTF8fastFreeEPv(children.buffer);
        return;
    }

    auto* const destSlot = reinterpret_cast<unsigned char*>(children.buffer) + static_cast<std::size_t>(children.size) * sizeof(HistoryItem*);
    const GuestArena::HostWrite destWrite(destSlot, sizeof(HistoryItem*));
    if (!destWrite.Open() || !GuestWritable_nid_no_patch(destSlot, sizeof(HistoryItem*)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!GuestCopyOut_nid_no_patch(destSlot, &childPtr, sizeof(childPtr)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const HistoryItemVectorStorage updated{children.buffer, children.capacity, children.size + 1};
    if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(self) + offsetof(HistoryItem, children), &updated, sizeof(updated)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!GuestCopyOut_nid_no_patch(refSlot, &nullChild, sizeof(nullChild))) {
        GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(self) + offsetof(HistoryItem, children), &children, sizeof(children));
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
}

void AssignUniquePointerField(HistoryItem* self, std::size_t fieldOffset, void** source, bool notify, const char* function) {
    RequireNonNullHistoryItem(self);
    if (!source) throw std::invalid_argument("null WTF::RefPtr");
    const WritableHistoryItem writable(self, function, true);
    void* const current = ReadGuestField<void*>(self, fieldOffset, function);
    void* const incoming = ReadGuestValue<void*>(source, function);
    if (current && incoming != current) NotImplemented_nid_no_patch(function);
    unsigned char* const targetAddress = reinterpret_cast<unsigned char*>(self) + fieldOffset;
    if (reinterpret_cast<unsigned char*>(source) != targetAddress) {
        const GuestArena::HostWrite sourceWrite(source, sizeof(void*));
        if (!sourceWrite.Open() || !GuestWritable_nid_no_patch(source, sizeof(void*)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        if (!GuestCopyOut_nid_no_patch(targetAddress, &incoming, sizeof(incoming)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        void* const nullPtr = nullptr;
        if (!GuestCopyOut_nid_no_patch(source, &nullPtr, sizeof(nullPtr))) {
            GuestCopyOut_nid_no_patch(targetAddress, &current, sizeof(current));
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
    }
    if (notify) historyItemChangedHook(self);
}

template <typename T>
void WriteScalarField(HistoryItem* self, std::size_t fieldOffset, const T& value, bool notify, const char* function) {
    const WritableHistoryItem writable(self, function, false);
    if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(self) + fieldOffset, &value, sizeof(value)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (notify) historyItemChangedHook(self);
}

}

namespace NKWebKit::WebCoreAbi {

HistoryItemChangedHook historyItemChangedHook = &HistoryItemChangedNoOp;

}

extern "C" {

const String& APS5_VABI _ZNK7WebCore11HistoryItem9urlStringEv(const HistoryItem* self) {
    return RequireReadableHistoryItem(self, __func__)->urlString;
}

const String& APS5_VABI _ZNK7WebCore11HistoryItem17originalURLStringEv(const HistoryItem* self) {
    return RequireReadableHistoryItem(self, __func__)->originalURLString;
}

const String& APS5_VABI _ZNK7WebCore11HistoryItem5titleEv(const HistoryItem* self) {
    return RequireReadableHistoryItem(self, __func__)->title;
}

const String& APS5_VABI _ZNK7WebCore11HistoryItem14alternateTitleEv(const HistoryItem* self) {
    return RequireReadableHistoryItem(self, __func__)->displayTitle;
}

const String& APS5_VABI _ZNK7WebCore11HistoryItem8referrerEv(const HistoryItem* self) {
    return RequireReadableHistoryItem(self, __func__)->referrer;
}

const String& APS5_VABI _ZNK7WebCore11HistoryItem6targetEv(const HistoryItem* self) {
    return RequireReadableHistoryItem(self, __func__)->target;
}

const IntPoint& APS5_VABI _ZNK7WebCore11HistoryItem14scrollPositionEv(const HistoryItem* self) {
    return RequireReadableHistoryItem(self, __func__)->scrollPosition;
}

float APS5_VABI _ZNK7WebCore11HistoryItem15pageScaleFactorEv(const HistoryItem* self) {
    RequireReadableHistoryItem(self, __func__);
    return ReadGuestField<float>(self, offsetof(HistoryItem, pageScaleFactor), __func__);
}

bool APS5_VABI _ZNK7WebCore11HistoryItem12isTargetItemEv(const HistoryItem* self) {
    RequireReadableHistoryItem(self, __func__);
    return ReadGuestField<bool>(self, offsetof(HistoryItem, isTargetItem), __func__);
}

bool APS5_VABI _ZNK7WebCore11HistoryItem27shouldRestoreScrollPositionEv(const HistoryItem* self) {
    RequireReadableHistoryItem(self, __func__);
    return ReadGuestField<bool>(self, offsetof(HistoryItem, shouldRestoreScrollPosition), __func__);
}

std::uint8_t APS5_VABI _ZNK7WebCore11HistoryItem28shouldOpenExternalURLsPolicyEv(const HistoryItem* self) {
    RequireReadableHistoryItem(self, __func__);
    return ReadGuestField<std::uint8_t>(self, offsetof(HistoryItem, shouldOpenExternalURLsPolicy), __func__);
}

const HistoryItemVectorStorage& APS5_VABI _ZNK7WebCore11HistoryItem13documentStateEv(const HistoryItem* self) {
    return RequireReadableHistoryItem(self, __func__)->documentState;
}

const HistoryItemVectorStorage& APS5_VABI _ZNK7WebCore11HistoryItem8childrenEv(const HistoryItem* self) {
    return RequireReadableHistoryItem(self, __func__)->children;
}

String APS5_VABI _ZNK7WebCore11HistoryItem15formContentTypeEv(const HistoryItem* self) {
    RequireReadableHistoryItem(self, __func__);
    auto* const impl = ReadGuestField<StringImpl*>(self, offsetof(HistoryItem, formContentType), __func__);
    if (!impl) return String();
    std::vector<PlannedStringState> planned;
    std::deque<GuestArena::HostWrite> stringWrites;
    PlanRetain(impl, __func__, planned, stringWrites);
    std::vector<std::pair<StringImpl*, unsigned>> retained;
    ApplyRetains({impl}, planned, retained, __func__);
    return String(impl);
}

bool APS5_VABI _ZNK7WebCore11HistoryItem11hasChildrenEv(const HistoryItem* self) {
    RequireReadableHistoryItem(self, __func__);
    return ReadGuestField<HistoryItemVectorStorage>(self, offsetof(HistoryItem, children), __func__).size != 0;
}

bool APS5_VABI _ZNK7WebCore11HistoryItem20hasCachedPageExpiredEv(const HistoryItem* self) {
    RequireReadableHistoryItem(self, __func__);
    const void* const cachedPage = ReadGuestField<void*>(self, offsetof(HistoryItem, cachedPage), __func__);
    if (!cachedPage) return false;
    const auto page = ReadGuestValue<CachedPage>(cachedPage, __func__);
    return _ZN3WTF13MonotonicTime3nowEv().m_value > page.expirationTime;
}

void* APS5_VABI _ZN7WebCore11HistoryItem8formDataEv(HistoryItem* self) {
    RequireReadableHistoryItem(self, __func__);
    return ReadGuestField<void*>(self, offsetof(HistoryItem, formData), __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem17setAlternateTitleERKN3WTF6StringE(HistoryItem* self, const String* value) {
    AssignFieldString(self, offsetof(HistoryItem, displayTitle), value, true, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem12setURLStringERKN3WTF6StringE(HistoryItem* self, const String* value) {
    AssignFieldString(self, offsetof(HistoryItem, urlString), value, true, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem20setOriginalURLStringERKN3WTF6StringE(HistoryItem* self, const String* value) {
    AssignFieldString(self, offsetof(HistoryItem, originalURLString), value, true, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem11setReferrerERKN3WTF6StringE(HistoryItem* self, const String* value) {
    AssignFieldString(self, offsetof(HistoryItem, referrer), value, true, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem8setTitleERKN3WTF6StringE(HistoryItem* self, const String* value) {
    AssignFieldString(self, offsetof(HistoryItem, title), value, true, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem18setFormContentTypeERKN3WTF6StringE(HistoryItem* self, const String* value) {
    AssignFieldString(self, offsetof(HistoryItem, formContentType), value, false, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem17setScrollPositionERKNS_8IntPointE(HistoryItem* self, const IntPoint* value) {
    RequireNonNullHistoryItem(self);
    if (!value) throw std::invalid_argument("null WebCore::IntPoint");
    const WritableHistoryItem writable(self, __func__, false);
    const auto point = ReadGuestValue<IntPoint>(value, __func__);
    if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(self) + offsetof(HistoryItem, scrollPosition), &point, sizeof(point)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore11HistoryItem18setPageScaleFactorEf(HistoryItem* self, float value) {
    WriteScalarField(self, offsetof(HistoryItem, pageScaleFactor), value, false, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem15setIsTargetItemEb(HistoryItem* self, bool value) {
    WriteScalarField(self, offsetof(HistoryItem, isTargetItem), value, false, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem30setShouldRestoreScrollPositionEb(HistoryItem* self, bool value) {
    WriteScalarField(self, offsetof(HistoryItem, shouldRestoreScrollPosition), value, true, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem31setShouldOpenExternalURLsPolicyENS_28ShouldOpenExternalURLsPolicyE(HistoryItem* self, std::uint8_t value) {
    WriteScalarField(self, offsetof(HistoryItem, shouldOpenExternalURLsPolicy), value, false, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem9setTargetERKN3WTF6StringE(HistoryItem* self, const String* value) {
    AssignFieldString(self, offsetof(HistoryItem, target), value, true, __func__);
}

HistoryItem* APS5_VABI _ZN7WebCore11HistoryItem19childItemWithTargetERKN3WTF6StringE(HistoryItem* self, const String* target) {
    RequireNonNullHistoryItem(self);
    if (!target) throw std::invalid_argument("null WTF::String");
    RequireReadableHistoryItem(self, __func__);
    auto* const keyImpl = ReadGuestValue<StringImpl*>(target, __func__);
    ValidateReadableStringImpl(keyImpl, __func__);
    const auto children = ReadGuestField<HistoryItemVectorStorage>(self, offsetof(HistoryItem, children), __func__);
    if (children.size > children.capacity)
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!children.size) return nullptr;
    if (!children.buffer || children.size > std::numeric_limits<std::size_t>::max() / sizeof(HistoryItem*))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const std::size_t bytes = static_cast<std::size_t>(children.size) * sizeof(HistoryItem*);
    RequireReadableGuestBytes(children.buffer, bytes, __func__);
    std::vector<HistoryItem*> childPointers(children.size, nullptr);
    if (!GuestCopyIn_nid_no_patch(childPointers.data(), children.buffer, bytes))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    for (HistoryItem* child : childPointers) {
        if (!child) continue;
        RequireReadableGuestBytes(child, sizeof(HistoryItem), __func__);
        auto* const childTargetImpl = ReadGuestField<StringImpl*>(child, offsetof(HistoryItem, target), __func__);
        ValidateReadableStringImpl(childTargetImpl, __func__);
        if (_ZN3WTF5equalEPKNS_10StringImplES2_(childTargetImpl, keyImpl)) return child;
    }
    return nullptr;
}

void APS5_VABI _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_S4_NS_25BackForwardItemIdentifierE(HistoryItem* self, const String* urlString, const String* title, const String* alternateTitle, NKWebKit::WebCoreAbi::BackForwardItemIdentifier identifier) {
    Construct(self, urlString, title, alternateTitle, identifier, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_NS_25BackForwardItemIdentifierE(HistoryItem* self, const String* urlString, const String* title, const String* alternateTitle, NKWebKit::WebCoreAbi::BackForwardItemIdentifier identifier) {
    Construct(self, urlString, title, alternateTitle, identifier, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem16setDocumentStateERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(HistoryItem* self, const HistoryItemVectorStorage* state) {
    AssignDocumentState(self, state, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem12addChildItemEON3WTF3RefIS0_NS1_13DumbPtrTraitsIS0_EEEE(HistoryItem* self, HistoryItem** child) {
    AppendChild(self, child, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem14setStateObjectEON3WTF6RefPtrINS_21SerializedScriptValueENS1_13DumbPtrTraitsIS3_EEEE(HistoryItem* self, void** object) {
    AssignUniquePointerField(self, offsetof(HistoryItem, stateObject), object, true, __func__);
}

void APS5_VABI _ZN7WebCore11HistoryItem11setFormDataEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(HistoryItem* self, void** formData) {
    AssignUniquePointerField(self, offsetof(HistoryItem, formData), formData, false, __func__);
}

}
