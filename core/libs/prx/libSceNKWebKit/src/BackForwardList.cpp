#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <algorithm>

extern "C" {

const void* APS5_VABI WKBackForwardListItemCopyOriginalURL(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::BackForwardListItem>(itemRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(item->originalURL));
}

const void* APS5_VABI WKBackForwardListItemCopyURL(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::BackForwardListItem>(itemRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(item->url));
}

const void* APS5_VABI WKBackForwardListItemCopyTitle(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::BackForwardListItem>(itemRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(item->title));
}

const void* APS5_VABI WKBackForwardListGetCurrentItem(const void* listRef) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    return list->currentItem.Get();
}

const void* APS5_VABI WKBackForwardListGetBackItem(const void* listRef) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    if (list->backList.empty()) return nullptr;
    return list->backList.front().Get();
}

const void* APS5_VABI WKBackForwardListGetForwardItem(const void* listRef) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    if (list->forwardList.empty()) return nullptr;
    return list->forwardList.front().Get();
}

const void* APS5_VABI WKBackForwardListGetItemAtIndex(const void* listRef, std::int32_t index) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    if (index < 0) {
        std::size_t backIndex = static_cast<std::size_t>(-index - 1);
        if (backIndex < list->backList.size()) return list->backList[backIndex].Get();
    } else if (index > 0) {
        std::size_t forwardIndex = static_cast<std::size_t>(index - 1);
        if (forwardIndex < list->forwardList.size()) return list->forwardList[forwardIndex].Get();
    } else {
        return list->currentItem.Get();
    }
    return nullptr;
}

std::uint32_t APS5_VABI WKBackForwardListGetBackListCount(const void* listRef) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    return static_cast<std::uint32_t>(list->backList.size());
}

std::uint32_t APS5_VABI WKBackForwardListGetForwardListCount(const void* listRef) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    return static_cast<std::uint32_t>(list->forwardList.size());
}

void APS5_VABI WKBackForwardListClear(const void* listRef) {
    auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    list->currentItem = nullptr;
    list->backList.clear();
    list->forwardList.clear();
}

const void* APS5_VABI WKBackForwardListCopyBackListWithLimit(const void* listRef, std::uint32_t limit) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    std::size_t count = std::min<std::size_t>(list->backList.size(), limit);
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        elements.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(list->backList[i].Get()));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(elements)));
}

const void* APS5_VABI WKBackForwardListCopyForwardListWithLimit(const void* listRef, std::uint32_t limit) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    std::size_t count = std::min<std::size_t>(list->forwardList.size(), limit);
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        elements.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(list->forwardList[i].Get()));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(elements)));
}

}
