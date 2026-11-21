#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageCountStringMatches(const void* pageRef, const void* stringRef, std::uint32_t findOptions, unsigned maxMatchCount) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* str = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    if (page->isClosed) return;
    page->activeCountMatches = NKWebKit::Page::FindStringState{ str->characters, findOptions, maxMatchCount };
}

void APS5_VABI WKPageFindString(const void* pageRef, const void* stringRef, std::uint32_t findOptions, unsigned maxMatchCount) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* str = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    if (page->isClosed) return;
    page->activeFindString = NKWebKit::Page::FindStringState{ str->characters, findOptions, maxMatchCount };
    page->findUIHidden = false;
}

void APS5_VABI WKPageFindStringMatches(const void* pageRef, const void* stringRef, std::uint32_t findOptions, unsigned maxMatchCount) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* str = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    if (page->isClosed) return;
    page->activeFindMatches = NKWebKit::Page::FindStringState{ str->characters, findOptions, maxMatchCount };
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
