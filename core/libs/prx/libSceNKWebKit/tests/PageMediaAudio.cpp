#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "PageMediaAudio check failed at line %u\n", location.line());
        std::abort();
    }
}

}

int main() {
    auto config = NKWebKit::Make<NKWebKit::PageConfiguration>();
    auto page = NKWebKit::Make<NKWebKit::Page>(std::move(config));
    const void* pageRef = page.Get();

    Require(!WKPageIsPlayingAudio(pageRef));

    WKPageSetMediaVolume(pageRef, 0.5f);
    Require(page->mediaVolume == 0.5f);

    WKPageSetMuted(pageRef, true);
    Require(page->muted == true);

    WKPageSetMuted(pageRef, false);
    Require(page->muted == false);

    bool threw = false;
    try {
        WKPageSetMediaVolume(nullptr, 0.8f);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKPageSetMuted(nullptr, true);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKPageIsPlayingAudio(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    return 0;
}
