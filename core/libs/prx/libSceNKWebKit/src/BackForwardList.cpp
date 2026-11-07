#include "prx/libSceNKWebKit/src/BackForwardList.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <algorithm>

namespace {

std::optional<std::size_t> CurrentIndex(const NKWebKit::BackForwardList* list) {
    if (!list->attached || !list->currentIndex || *list->currentIndex >= list->entries.size()) return std::nullopt;
    return list->currentIndex;
}

std::uint32_t BackCount(const NKWebKit::BackForwardList* list) {
    auto current = CurrentIndex(list);
    return current ? static_cast<std::uint32_t>(*current) : 0;
}

std::uint32_t ForwardCount(const NKWebKit::BackForwardList* list) {
    auto current = CurrentIndex(list);
    return current ? static_cast<std::uint32_t>(list->entries.size() - (*current + 1)) : 0;
}

const void* ItemAt(const NKWebKit::BackForwardList* list, std::size_t index) {
    return list->entries[index].Get();
}

const void* CopyURL(const std::optional<std::string>& url) {
    if (!url) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(*url));
}

const void* CopyItems(const NKWebKit::BackForwardList* list, std::size_t first, std::size_t count) {
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(count);
    for (std::size_t index = first; index < first + count; ++index) {
        elements.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(list->entries[index].Get()));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(elements)));
}

}  // namespace

extern "C" {

const void* APS5_VABI WKBackForwardListGetCurrentItem(const void* listRef) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    auto current = CurrentIndex(list);
    return current ? ItemAt(list, *current) : nullptr;
}

const void* APS5_VABI WKBackForwardListGetBackItem(const void* listRef) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    auto current = CurrentIndex(list);
    return current && *current ? ItemAt(list, *current - 1) : nullptr;
}

const void* APS5_VABI WKBackForwardListGetForwardItem(const void* listRef) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    auto current = CurrentIndex(list);
    return current && *current < list->entries.size() - 1 ? ItemAt(list, *current + 1) : nullptr;
}

const void* APS5_VABI WKBackForwardListGetItemAtIndex(const void* listRef, int index) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    auto current = CurrentIndex(list);
    if (!current) return nullptr;
    if (index < 0 && static_cast<std::int64_t>(-static_cast<std::int64_t>(index)) > BackCount(list)) return nullptr;
    if (index > 0 && static_cast<std::uint32_t>(index) > ForwardCount(list)) return nullptr;
    return ItemAt(list, static_cast<std::size_t>(static_cast<std::int64_t>(*current) + index));
}

void APS5_VABI WKBackForwardListClear(const void* listRef) {
    auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
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

std::uint32_t APS5_VABI WKBackForwardListGetBackListCount(const void* listRef) {
    return BackCount(NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__));
}

std::uint32_t APS5_VABI WKBackForwardListGetForwardListCount(const void* listRef) {
    return ForwardCount(NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__));
}

const void* APS5_VABI WKBackForwardListCopyBackListWithLimit(const void* listRef, std::uint32_t limit) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    std::size_t back = BackCount(list);
    std::size_t size = std::min<std::size_t>(back, limit);
    return CopyItems(list, back - size, size);
}

const void* APS5_VABI WKBackForwardListCopyForwardListWithLimit(const void* listRef, std::uint32_t limit) {
    const auto* list = NKWebKit::Cast<NKWebKit::BackForwardList>(listRef, __func__);
    auto current = CurrentIndex(list);
    std::size_t size = std::min<std::size_t>(ForwardCount(list), limit);
    return CopyItems(list, current ? *current + 1 : 0, size);
}

const void* APS5_VABI WKBackForwardListItemCopyURL(const void* itemRef) {
    return CopyURL(NKWebKit::Cast<NKWebKit::BackForwardListItem>(itemRef, __func__)->url);
}

const void* APS5_VABI WKBackForwardListItemCopyOriginalURL(const void* itemRef) {
    return CopyURL(NKWebKit::Cast<NKWebKit::BackForwardListItem>(itemRef, __func__)->originalURL);
}

const void* APS5_VABI WKBackForwardListItemCopyTitle(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::BackForwardListItem>(itemRef, __func__);
    if (!item->title) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string()));
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(item->title.Get()));
}

}
