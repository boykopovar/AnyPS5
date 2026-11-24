#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

namespace {

unsigned CountOccurrences(const NKWebKit::Page* page, const std::u16string& target, unsigned maxCount) {
    if (target.empty()) {
        return 0;
    }
    unsigned count = 0;
    auto searchIn = [&](const std::optional<std::u16string>& text) {
        if (!text || (maxCount > 0 && count >= maxCount)) {
            return;
        }
        std::size_t pos = 0;
        while ((pos = text->find(target, pos)) != std::u16string::npos) {
            count++;
            pos += target.length();
            if (maxCount > 0 && count >= maxCount) {
                break;
            }
        }
    };
    searchIn(page->title);
    return count;
}

}

extern "C" {

void APS5_VABI WKPageCountStringMatches(const void* pageRef, const void* stringRef, std::uint32_t findOptions, unsigned maxMatchCount) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* str = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    if (page->isClosed) return;
    const unsigned matches = CountOccurrences(page, str->characters, maxMatchCount);
    page->activeCountMatches = NKWebKit::Page::FindStringState{ str->characters, findOptions, maxMatchCount, matches };
}

void APS5_VABI WKPageFindString(const void* pageRef, const void* stringRef, std::uint32_t findOptions, unsigned maxMatchCount) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* str = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    if (page->isClosed) return;
    const unsigned matches = CountOccurrences(page, str->characters, maxMatchCount);
    page->activeFindString = NKWebKit::Page::FindStringState{ str->characters, findOptions, maxMatchCount, matches };
    page->findUIHidden = false;
    if (matches > 0) {
        page->selectedFindMatch = 0;
    }
}

void APS5_VABI WKPageFindStringMatches(const void* pageRef, const void* stringRef, std::uint32_t findOptions, unsigned maxMatchCount) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* str = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    if (page->isClosed) return;
    const unsigned matches = CountOccurrences(page, str->characters, maxMatchCount);
    page->activeFindMatches = NKWebKit::Page::FindStringState{ str->characters, findOptions, maxMatchCount, matches };
}

void APS5_VABI WKPageGetImageForFindMatch(const void* pageRef, std::uint32_t matchIndex) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed) return;
    page->lastImageFindMatch = matchIndex;
}

void APS5_VABI WKPageHideFindUI(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed) return;
    page->findUIHidden = true;
}

void APS5_VABI WKPageSelectFindMatch(const void* pageRef, std::uint32_t matchIndex) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed) return;
    page->selectedFindMatch = matchIndex;
}

}
