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

void HistoryItemChangedNoOp(HistoryItem*) {
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

}
