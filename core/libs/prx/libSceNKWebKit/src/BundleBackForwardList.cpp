#include "prx/libSceNKWebKit/src/BundleBackForwardList.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <algorithm>
#include <utility>
#include <vector>

namespace {

std::optional<std::size_t> CurrentIndex(const NKWebKit::BundleBackForwardList* list) {
    if (!list->attached || !list->currentIndex || *list->currentIndex >= list->entries.size()) return std::nullopt;
    return list->currentIndex;
}

std::uint32_t BackCount(const NKWebKit::BundleBackForwardList* list) {
    auto current = CurrentIndex(list);
    return current ? static_cast<std::uint32_t>(*current) : 0;
}

std::uint32_t ForwardCount(const NKWebKit::BundleBackForwardList* list) {
    auto current = CurrentIndex(list);
    return current ? static_cast<std::uint32_t>(list->entries.size() - (*current + 1)) : 0;
}

}

extern "C" {

void APS5_VABI WKBundleBackForwardListClear(const void* listRef) {
    auto* list = NKWebKit::Cast<NKWebKit::BundleBackForwardList>(listRef, __func__);
    if (!list->attached || list->entries.size() <= 1) return;
    auto current = CurrentIndex(list);
    if (!current) {
        list->entries.clear();
        list->currentIndex = std::nullopt;
        return;
    }
    auto item = list->entries[*current];
    list->entries.clear();
    list->entries.push_back(std::move(item));
    list->currentIndex = 0;
}

const void* APS5_VABI WKBundleBackForwardListCopyItemAtIndex(const void* listRef, int index) {
    const auto* list = NKWebKit::Cast<NKWebKit::BundleBackForwardList>(listRef, __func__);
    auto current = CurrentIndex(list);
    if (!current) return nullptr;
    if (index < 0 && static_cast<std::int64_t>(-static_cast<std::int64_t>(index)) > BackCount(list)) return nullptr;
    if (index > 0 && static_cast<std::uint32_t>(index) > ForwardCount(list)) return nullptr;
    std::size_t targetIndex = static_cast<std::size_t>(static_cast<std::int64_t>(*current) + index);
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::BundleBackForwardListItem>::Share(list->entries[targetIndex].Get()));
}

std::uint32_t APS5_VABI WKBundleBackForwardListGetBackListCount(const void* listRef) {
    return BackCount(NKWebKit::Cast<NKWebKit::BundleBackForwardList>(listRef, __func__));
}

std::uint32_t APS5_VABI WKBundleBackForwardListGetForwardListCount(const void* listRef) {
    return ForwardCount(NKWebKit::Cast<NKWebKit::BundleBackForwardList>(listRef, __func__));
}

const void* APS5_VABI WKBundleBackForwardListItemCopyChildren(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::BundleBackForwardListItem>(itemRef, __func__);
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(item->children.size());
    for (const auto& child : item->children) {
        elements.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(child.Get()));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(elements)));
}

const void* APS5_VABI WKBundleBackForwardListItemCopyOriginalURL(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::BundleBackForwardListItem>(itemRef, __func__);
    if (!item->originalURL) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(*item->originalURL));
}

const void* APS5_VABI WKBundleBackForwardListItemCopyTarget(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::BundleBackForwardListItem>(itemRef, __func__);
    if (!item->target) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string()));
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(item->target.Get()));
}

const void* APS5_VABI WKBundleBackForwardListItemCopyTitle(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::BundleBackForwardListItem>(itemRef, __func__);
    if (!item->title) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string()));
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(item->title.Get()));
}

const void* APS5_VABI WKBundleBackForwardListItemCopyURL(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::BundleBackForwardListItem>(itemRef, __func__);
    if (!item->url) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(*item->url));
}

bool APS5_VABI WKBundleBackForwardListItemHasCachedPageExpired(const void* itemRef) {
    return NKWebKit::Cast<NKWebKit::BundleBackForwardListItem>(itemRef, __func__)->hasCachedPageExpired;
}

bool APS5_VABI WKBundleBackForwardListItemIsInBackForwardCache(const void* itemRef) {
    return NKWebKit::Cast<NKWebKit::BundleBackForwardListItem>(itemRef, __func__)->inBackForwardCache;
}

bool APS5_VABI WKBundleBackForwardListItemIsSame(const void* item1Ref, const void* item2Ref) {
    const auto* item1 = NKWebKit::Cast<NKWebKit::BundleBackForwardListItem>(item1Ref, __func__);
    const auto* item2 = NKWebKit::Cast<NKWebKit::BundleBackForwardListItem>(item2Ref, __func__);
    if (item1 == item2) return true;
    if (item1->identifier != 0 && item1->identifier == item2->identifier) return true;
    return false;
}

bool APS5_VABI WKBundleBackForwardListItemIsTargetItem(const void* itemRef) {
    return NKWebKit::Cast<NKWebKit::BundleBackForwardListItem>(itemRef, __func__)->isTargetItem;
}

}
