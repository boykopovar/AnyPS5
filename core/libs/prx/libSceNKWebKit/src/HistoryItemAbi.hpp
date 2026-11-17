#pragma once

#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct HistoryItemVectorStorage {
    void* buffer;
    unsigned capacity;
    unsigned size;
};

struct CachedPage {
    void* page;
    double expirationTime;
};

struct HistoryItem {
    unsigned refCount{};
    WtfAbi::String urlString;
    WtfAbi::String originalURLString;
    WtfAbi::String referrer;
    WtfAbi::String target;
    WtfAbi::String title;
    WtfAbi::String displayTitle;
    WebCore::IntPoint scrollPosition{};
    float pageScaleFactor{};
    HistoryItemVectorStorage documentState{};
    std::uint8_t shouldOpenExternalURLsPolicy{};
    HistoryItemVectorStorage children{};
    bool lastVisitWasFailure{};
    bool isTargetItem{};
    bool wasRestoredFromSession{};
    bool wasCreatedByJSWithoutUserInteraction{};
    bool shouldRestoreScrollPosition{};
    std::int64_t itemSequenceNumber{};
    std::int64_t documentSequenceNumber{};
    void* stateObject{};
    void* formData{};
    WtfAbi::String formContentType;
    void* cachedPage{};
    std::uint8_t pruningReason{};
};

static_assert(sizeof(HistoryItemVectorStorage) == 16);
static_assert(offsetof(CachedPage, page) == 0);
static_assert(offsetof(CachedPage, expirationTime) == 8);
static_assert(offsetof(HistoryItem, refCount) == 0);
static_assert(offsetof(HistoryItem, urlString) == 8);
static_assert(offsetof(HistoryItem, originalURLString) == 16);
static_assert(offsetof(HistoryItem, referrer) == 24);
static_assert(offsetof(HistoryItem, target) == 32);
static_assert(offsetof(HistoryItem, title) == 40);
static_assert(offsetof(HistoryItem, displayTitle) == 48);
static_assert(offsetof(HistoryItem, scrollPosition) == 56);
static_assert(offsetof(HistoryItem, pageScaleFactor) == 64);
static_assert(offsetof(HistoryItem, documentState) == 72);
static_assert(offsetof(HistoryItem, shouldOpenExternalURLsPolicy) == 88);
static_assert(offsetof(HistoryItem, children) == 96);
static_assert(offsetof(HistoryItem, lastVisitWasFailure) == 112);
static_assert(offsetof(HistoryItem, isTargetItem) == 113);
static_assert(offsetof(HistoryItem, wasRestoredFromSession) == 114);
static_assert(offsetof(HistoryItem, wasCreatedByJSWithoutUserInteraction) == 115);
static_assert(offsetof(HistoryItem, shouldRestoreScrollPosition) == 116);
static_assert(offsetof(HistoryItem, itemSequenceNumber) == 120);
static_assert(offsetof(HistoryItem, documentSequenceNumber) == 128);
static_assert(offsetof(HistoryItem, stateObject) == 136);
static_assert(offsetof(HistoryItem, formData) == 144);
static_assert(offsetof(HistoryItem, formContentType) == 152);
static_assert(offsetof(HistoryItem, cachedPage) == 160);
static_assert(offsetof(HistoryItem, pruningReason) == 168);

using HistoryItemChangedHook = void (*)(HistoryItem*);
extern HistoryItemChangedHook historyItemChangedHook;

}
