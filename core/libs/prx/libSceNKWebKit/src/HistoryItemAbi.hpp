#pragma once

#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

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

struct BackForwardItemIdentifier {
    std::uint64_t object{};
    std::uint64_t processIdentifier{};
};

struct ContentSecurityPolicyResponseHeaders {
    HistoryItemVectorStorage headers{};
    int httpStatusCode{};
    bool emptyForMarkable{};
};

struct CrossOriginEmbedderPolicy {
    bool value{};
    bool reportOnlyValue{};
    WtfAbi::String reportingEndpoint;
    WtfAbi::String reportOnlyReportingEndpoint;
};

struct CrossOriginOpenerPolicy {
    std::uint8_t value{};
    std::uint8_t reportOnlyValue{};
    WtfAbi::String reportingEndpoint;
    WtfAbi::String reportOnlyReportingEndpoint;
};

struct PolicyContainer {
    ContentSecurityPolicyResponseHeaders contentSecurityPolicyResponseHeaders;
    CrossOriginEmbedderPolicy crossOriginEmbedderPolicy;
    CrossOriginOpenerPolicy crossOriginOpenerPolicy;
    std::uint8_t referrerPolicy{};
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
    std::int32_t pruningReason{};
    BackForwardItemIdentifier identifier{};
    PolicyContainer policyContainer;
    bool policyContainerEngaged{};

    void ref() { ++refCount; }
    void deref();
};

class HistoryItemRef {
public:
    explicit HistoryItemRef(HistoryItem& item) : m_item(&item) {}
    HistoryItemRef(const HistoryItemRef& other) : m_item(other.m_item) {
        if (m_item) m_item->ref();
    }
    HistoryItemRef(HistoryItemRef&& other) noexcept : m_item(std::exchange(other.m_item, nullptr)) {}
    HistoryItemRef& operator=(HistoryItemRef other) noexcept {
        std::swap(m_item, other.m_item);
        return *this;
    }
    ~HistoryItemRef() {
        if (m_item) m_item->deref();
    }
    HistoryItem* get() const { return m_item; }
    HistoryItem* operator->() const { return m_item; }
    HistoryItem& operator*() const { return *m_item; }

private:
    HistoryItem* m_item;
};

static_assert(sizeof(HistoryItemVectorStorage) == 16);
static_assert(sizeof(BackForwardItemIdentifier) == 16);
static_assert(offsetof(BackForwardItemIdentifier, object) == 0);
static_assert(offsetof(BackForwardItemIdentifier, processIdentifier) == 8);
static_assert(sizeof(ContentSecurityPolicyResponseHeaders) == 24);
static_assert(offsetof(ContentSecurityPolicyResponseHeaders, headers) == 0);
static_assert(offsetof(ContentSecurityPolicyResponseHeaders, httpStatusCode) == 16);
static_assert(offsetof(ContentSecurityPolicyResponseHeaders, emptyForMarkable) == 20);
static_assert(sizeof(CrossOriginEmbedderPolicy) == 24);
static_assert(offsetof(CrossOriginEmbedderPolicy, value) == 0);
static_assert(offsetof(CrossOriginEmbedderPolicy, reportOnlyValue) == 1);
static_assert(offsetof(CrossOriginEmbedderPolicy, reportingEndpoint) == 8);
static_assert(offsetof(CrossOriginEmbedderPolicy, reportOnlyReportingEndpoint) == 16);
static_assert(sizeof(CrossOriginOpenerPolicy) == 24);
static_assert(offsetof(CrossOriginOpenerPolicy, value) == 0);
static_assert(offsetof(CrossOriginOpenerPolicy, reportOnlyValue) == 1);
static_assert(offsetof(CrossOriginOpenerPolicy, reportingEndpoint) == 8);
static_assert(offsetof(CrossOriginOpenerPolicy, reportOnlyReportingEndpoint) == 16);
static_assert(sizeof(PolicyContainer) == 80);
static_assert(offsetof(PolicyContainer, contentSecurityPolicyResponseHeaders) == 0);
static_assert(offsetof(PolicyContainer, crossOriginEmbedderPolicy) == 24);
static_assert(offsetof(PolicyContainer, crossOriginOpenerPolicy) == 48);
static_assert(offsetof(PolicyContainer, referrerPolicy) == 72);
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
static_assert(offsetof(HistoryItem, identifier) == 176);
static_assert(offsetof(HistoryItem, policyContainer) == 192);
static_assert(offsetof(HistoryItem, policyContainerEngaged) == 272);
static_assert(sizeof(HistoryItem) == 280);
static_assert(sizeof(HistoryItemRef) == 8);
static_assert(!std::is_trivially_destructible_v<HistoryItemRef>);

using HistoryItemChangedHook = void (*)(HistoryItem*);
extern HistoryItemChangedHook historyItemChangedHook;

}
