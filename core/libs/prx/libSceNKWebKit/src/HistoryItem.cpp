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
#include "prx/libSceNKWebKit/include/HistoryItemExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::HistoryItem;
using NKWebKit::WebCoreAbi::HistoryItemVectorStorage;
using NKWebKit::WebCoreAbi::CachedPage;
using NKWebKit::WebCoreAbi::historyItemChangedHook;
using NKWebKit::WtfAbi::String;
using WebCore::IntPoint;

HistoryItem& Checked(HistoryItem* self) {
    if (!self) throw std::invalid_argument("null WebCore::HistoryItem");
    return *self;
}

const HistoryItem& Checked(const HistoryItem* self) {
    if (!self) throw std::invalid_argument("null WebCore::HistoryItem");
    return *self;
}

template <typename T>
const T& Checked(const T* value, const char* name) {
    if (!value) throw std::invalid_argument(name);
    return *value;
}

void Assign(String& target, const String& source) {
    String copy(source);
    std::swap(target.impl, copy.impl);
}

void InitString(String& target, const String& source) {
    target.impl = source.impl;
    if (target.impl) target.impl->refCount += 2;
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

void Construct(HistoryItem& item, const String* urlString, const String* title, const String* alternateTitle, const NKWebKit::WebCoreAbi::BackForwardItemIdentifier& identifier) {
    const String& url = Checked(urlString, "null WTF::String");
    const String& itemTitle = Checked(title, "null WTF::String");
    const String& itemAlternateTitle = Checked(alternateTitle, "null WTF::String");
    item.refCount = 1;
    InitString(item.urlString, url);
    InitString(item.originalURLString, url);
    item.referrer.impl = nullptr;
    item.target.impl = nullptr;
    InitString(item.title, itemTitle);
    InitString(item.displayTitle, itemAlternateTitle);
    item.scrollPosition = IntPoint{};
    item.pageScaleFactor = 0;
    item.documentState = {};
    item.shouldOpenExternalURLsPolicy = 0;
    item.children = {};
    item.lastVisitWasFailure = false;
    item.isTargetItem = false;
    item.wasRestoredFromSession = false;
    item.wasCreatedByJSWithoutUserInteraction = false;
    item.shouldRestoreScrollPosition = true;
    item.itemSequenceNumber = GenerateSequenceNumber();
    item.documentSequenceNumber = GenerateSequenceNumber();
    item.stateObject = nullptr;
    item.formData = nullptr;
    item.formContentType.impl = nullptr;
    item.cachedPage = nullptr;
    item.pruningReason = 0;
    item.identifier = identifier;
    item.policyContainer.engaged = 0;
}

void AssignDocumentState(HistoryItemVectorStorage& target, const HistoryItemVectorStorage& source) {
    if (&target == &source) return;
    auto* targetElements = static_cast<String*>(target.buffer);
    const String* sourceElements = static_cast<const String*>(source.buffer);
    if (target.size > source.size) {
        for (unsigned i = source.size; i < target.size; ++i) Release(targetElements[i].impl);
        target.size = source.size;
    } else if (source.size > target.capacity) {
        for (unsigned i = 0; i < target.size; ++i) Release(targetElements[i].impl);
        if (target.buffer) _ZN3WTF8fastFreeEPv(target.buffer);
        target.buffer = nullptr;
        target.capacity = 0;
        target.size = 0;
        if (source.size > 0) {
            target.buffer = _ZN3WTF10fastMallocEm(static_cast<std::size_t>(source.size) * sizeof(String));
            target.capacity = source.size;
        }
        targetElements = static_cast<String*>(target.buffer);
    }
    unsigned common = target.size;
    for (unsigned i = 0; i < common; ++i) {
        if (sourceElements[i].impl) sourceElements[i].impl->refCount += 2;
        Release(targetElements[i].impl);
        targetElements[i].impl = sourceElements[i].impl;
    }
    for (unsigned i = common; i < source.size; ++i) {
        if (sourceElements[i].impl) sourceElements[i].impl->refCount += 2;
        targetElements[i].impl = sourceElements[i].impl;
    }
    target.size = source.size;
}

void AppendChild(HistoryItemVectorStorage& children, HistoryItem** refSlot) {
    if (children.size == children.capacity) {
        std::size_t newMinCapacity = static_cast<std::size_t>(children.size) + 1;
        std::size_t growth = static_cast<std::size_t>(children.capacity) + children.capacity / 4 + 1;
        std::size_t newCapacity = std::max(newMinCapacity, std::max<std::size_t>(16, growth));
        auto* newBuffer = static_cast<HistoryItem**>(_ZN3WTF10fastMallocEm(newCapacity * sizeof(HistoryItem*)));
        if (children.buffer) {
            std::memcpy(newBuffer, children.buffer, static_cast<std::size_t>(children.size) * sizeof(HistoryItem*));
            _ZN3WTF8fastFreeEPv(children.buffer);
        }
        children.buffer = newBuffer;
        children.capacity = static_cast<unsigned>(newCapacity);
    }
    static_cast<HistoryItem**>(children.buffer)[children.size] = *refSlot;
    *refSlot = nullptr;
    ++children.size;
}

}

namespace NKWebKit::WebCoreAbi {

HistoryItemChangedHook historyItemChangedHook = &HistoryItemChangedNoOp;

}

extern "C" {

const String& APS5_VABI _ZNK7WebCore11HistoryItem9urlStringEv(const HistoryItem* self) {
    return Checked(self).urlString;
}

const String& APS5_VABI _ZNK7WebCore11HistoryItem17originalURLStringEv(const HistoryItem* self) {
    return Checked(self).originalURLString;
}

const String& APS5_VABI _ZNK7WebCore11HistoryItem5titleEv(const HistoryItem* self) {
    return Checked(self).title;
}

const String& APS5_VABI _ZNK7WebCore11HistoryItem14alternateTitleEv(const HistoryItem* self) {
    return Checked(self).displayTitle;
}

const String& APS5_VABI _ZNK7WebCore11HistoryItem8referrerEv(const HistoryItem* self) {
    return Checked(self).referrer;
}

const String& APS5_VABI _ZNK7WebCore11HistoryItem6targetEv(const HistoryItem* self) {
    return Checked(self).target;
}

const IntPoint& APS5_VABI _ZNK7WebCore11HistoryItem14scrollPositionEv(const HistoryItem* self) {
    return Checked(self).scrollPosition;
}

float APS5_VABI _ZNK7WebCore11HistoryItem15pageScaleFactorEv(const HistoryItem* self) {
    return Checked(self).pageScaleFactor;
}

bool APS5_VABI _ZNK7WebCore11HistoryItem12isTargetItemEv(const HistoryItem* self) {
    return Checked(self).isTargetItem;
}

bool APS5_VABI _ZNK7WebCore11HistoryItem27shouldRestoreScrollPositionEv(const HistoryItem* self) {
    return Checked(self).shouldRestoreScrollPosition;
}

std::uint8_t APS5_VABI _ZNK7WebCore11HistoryItem28shouldOpenExternalURLsPolicyEv(const HistoryItem* self) {
    return Checked(self).shouldOpenExternalURLsPolicy;
}

const HistoryItemVectorStorage& APS5_VABI _ZNK7WebCore11HistoryItem13documentStateEv(const HistoryItem* self) {
    return Checked(self).documentState;
}

const HistoryItemVectorStorage& APS5_VABI _ZNK7WebCore11HistoryItem8childrenEv(const HistoryItem* self) {
    return Checked(self).children;
}

String APS5_VABI _ZNK7WebCore11HistoryItem15formContentTypeEv(const HistoryItem* self) {
    return Checked(self).formContentType;
}

bool APS5_VABI _ZNK7WebCore11HistoryItem11hasChildrenEv(const HistoryItem* self) {
    return Checked(self).children.size != 0;
}

bool APS5_VABI _ZNK7WebCore11HistoryItem20hasCachedPageExpiredEv(const HistoryItem* self) {
    const auto* cachedPage = static_cast<const CachedPage*>(Checked(self).cachedPage);
    if (!cachedPage) return false;
    return _ZN3WTF13MonotonicTime3nowEv().m_value > cachedPage->expirationTime;
}

void* APS5_VABI _ZN7WebCore11HistoryItem8formDataEv(HistoryItem* self) {
    return Checked(self).formData;
}

void APS5_VABI _ZN7WebCore11HistoryItem17setAlternateTitleERKN3WTF6StringE(HistoryItem* self, const String* value) {
    auto& item = Checked(self);
    Assign(item.displayTitle, Checked(value, "null WTF::String"));
    historyItemChangedHook(&item);
}

void APS5_VABI _ZN7WebCore11HistoryItem12setURLStringERKN3WTF6StringE(HistoryItem* self, const String* value) {
    auto& item = Checked(self);
    Assign(item.urlString, Checked(value, "null WTF::String"));
    historyItemChangedHook(&item);
}

void APS5_VABI _ZN7WebCore11HistoryItem20setOriginalURLStringERKN3WTF6StringE(HistoryItem* self, const String* value) {
    auto& item = Checked(self);
    Assign(item.originalURLString, Checked(value, "null WTF::String"));
    historyItemChangedHook(&item);
}

void APS5_VABI _ZN7WebCore11HistoryItem11setReferrerERKN3WTF6StringE(HistoryItem* self, const String* value) {
    auto& item = Checked(self);
    Assign(item.referrer, Checked(value, "null WTF::String"));
    historyItemChangedHook(&item);
}

void APS5_VABI _ZN7WebCore11HistoryItem8setTitleERKN3WTF6StringE(HistoryItem* self, const String* value) {
    auto& item = Checked(self);
    Assign(item.title, Checked(value, "null WTF::String"));
    historyItemChangedHook(&item);
}

void APS5_VABI _ZN7WebCore11HistoryItem18setFormContentTypeERKN3WTF6StringE(HistoryItem* self, const String* value) {
    auto& item = Checked(self);
    Assign(item.formContentType, Checked(value, "null WTF::String"));
}

void APS5_VABI _ZN7WebCore11HistoryItem17setScrollPositionERKNS_8IntPointE(HistoryItem* self, const IntPoint* value) {
    auto& item = Checked(self);
    item.scrollPosition = Checked(value, "null WebCore::IntPoint");
}

void APS5_VABI _ZN7WebCore11HistoryItem18setPageScaleFactorEf(HistoryItem* self, float value) {
    Checked(self).pageScaleFactor = value;
}

void APS5_VABI _ZN7WebCore11HistoryItem15setIsTargetItemEb(HistoryItem* self, bool value) {
    Checked(self).isTargetItem = value;
}

void APS5_VABI _ZN7WebCore11HistoryItem30setShouldRestoreScrollPositionEb(HistoryItem* self, bool value) {
    auto& item = Checked(self);
    item.shouldRestoreScrollPosition = value;
    historyItemChangedHook(&item);
}

void APS5_VABI _ZN7WebCore11HistoryItem31setShouldOpenExternalURLsPolicyENS_28ShouldOpenExternalURLsPolicyE(HistoryItem* self, std::uint8_t value) {
    Checked(self).shouldOpenExternalURLsPolicy = value;
}

void APS5_VABI _ZN7WebCore11HistoryItem9setTargetERKN3WTF6StringE(HistoryItem* self, const String* value) {
    auto& item = Checked(self);
    Assign(item.target, Checked(value, "null WTF::String"));
    historyItemChangedHook(&item);
}

HistoryItem* APS5_VABI _ZN7WebCore11HistoryItem19childItemWithTargetERKN3WTF6StringE(HistoryItem* self, const String* target) {
    auto& item = Checked(self);
    const String& key = Checked(target, "null WTF::String");
    auto** children = static_cast<HistoryItem**>(item.children.buffer);
    for (unsigned index = 0; index < item.children.size; ++index) {
        HistoryItem* child = children[index];
        if (child && _ZN3WTF5equalEPKNS_10StringImplES2_(child->target.impl, key.impl)) return child;
    }
    return nullptr;
}

void APS5_VABI _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_S4_NS_25BackForwardItemIdentifierE(HistoryItem* self, const String* urlString, const String* title, const String* alternateTitle, NKWebKit::WebCoreAbi::BackForwardItemIdentifier identifier) {
    Construct(Checked(self), urlString, title, alternateTitle, identifier);
}

void APS5_VABI _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_NS_25BackForwardItemIdentifierE(HistoryItem* self, const String* urlString, const String* title, const String* alternateTitle, NKWebKit::WebCoreAbi::BackForwardItemIdentifier identifier) {
    Construct(Checked(self), urlString, title, alternateTitle, identifier);
}

void APS5_VABI _ZN7WebCore11HistoryItem16setDocumentStateERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(HistoryItem* self, const HistoryItemVectorStorage* state) {
    AssignDocumentState(Checked(self).documentState, Checked(state, "null WTF::Vector"));
}

void APS5_VABI _ZN7WebCore11HistoryItem12addChildItemEON3WTF3RefIS0_NS1_13DumbPtrTraitsIS0_EEEE(HistoryItem* self, HistoryItem** child) {
    if (!child) throw std::invalid_argument("null WTF::Ref");
    AppendChild(Checked(self).children, child);
}

void APS5_VABI _ZN7WebCore11HistoryItem14setStateObjectEON3WTF6RefPtrINS_21SerializedScriptValueENS1_13DumbPtrTraitsIS3_EEEE(HistoryItem* self, void** object) {
    auto& item = Checked(self);
    if (!object) throw std::invalid_argument("null WTF::RefPtr");
    if (item.stateObject && *object != item.stateObject) NotImplemented_nid_no_patch(__func__);
    if (object != &item.stateObject) {
        item.stateObject = *object;
        *object = nullptr;
    }
    historyItemChangedHook(&item);
}

void APS5_VABI _ZN7WebCore11HistoryItem11setFormDataEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(HistoryItem* self, void** formData) {
    auto& item = Checked(self);
    if (!formData) throw std::invalid_argument("null WTF::RefPtr");
    if (item.formData && *formData != item.formData) NotImplemented_nid_no_patch(__func__);
    if (formData != &item.formData) {
        item.formData = *formData;
        *formData = nullptr;
    }
}

}
