#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage load file check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);

    Require(!page->loadedFileURL);
    Require(!page->allowedDirectoryURL);
    Require(!page->provisionalURL);
    Require(!page->activeURL);
    Require(page->loadUserData == nullptr);

    auto fileURL = NKWebKit::Make<NKWebKit::URL>("file:///app/index.html");
    const void* fileURLRef = NKWebKit::ToRef(fileURL);
    auto allowedDir = NKWebKit::Make<NKWebKit::URL>("file:///app/");
    const void* allowedDirRef = NKWebKit::ToRef(allowedDir);

    auto initialCommitted = NKWebKit::Make<NKWebKit::URL>("https://example.com/initial");
    page->committedURL = initialCommitted;
    WKPageLoadFile(pageRef, fileURLRef, allowedDirRef);
    Require(page->committedURL.Get() == initialCommitted.Get());
    Require(static_cast<bool>(page->loadedFileURL));
    Require(page->loadedFileURL->text == "file:///app/index.html");
    Require(static_cast<bool>(page->allowedDirectoryURL));
    Require(page->allowedDirectoryURL->text == "file:///app/");
    Require(page->provisionalURL && page->provisionalURL->text == "file:///app/index.html");
    Require(page->activeURL && page->activeURL->text == "file:///app/index.html");
    Require(page->loadUserData == nullptr);

    WKPageLoadFile(pageRef, fileURLRef, nullptr);
    Require(static_cast<bool>(page->loadedFileURL));
    Require(page->loadedFileURL->text == "file:///app/index.html");
    Require(!page->allowedDirectoryURL);
    Require(page->provisionalURL && page->provisionalURL->text == "file:///app/index.html");
    Require(page->activeURL && page->activeURL->text == "file:///app/index.html");

    int userData = 654;
    WKPageLoadFileWithUserData(pageRef, fileURLRef, allowedDirRef, &userData);
    Require(static_cast<bool>(page->loadedFileURL));
    Require(static_cast<bool>(page->allowedDirectoryURL));
    Require(page->loadUserData == &userData);

    page->isClosed = true;
    page->loadedFileURL = nullptr;
    page->allowedDirectoryURL = nullptr;
    page->provisionalURL = nullptr;
    page->activeURL = nullptr;
    page->loadUserData = nullptr;

    WKPageLoadFile(pageRef, fileURLRef, allowedDirRef);
    Require(!page->loadedFileURL);
    Require(!page->allowedDirectoryURL);

    WKPageLoadFileWithUserData(pageRef, fileURLRef, allowedDirRef, &userData);
    Require(!page->loadedFileURL);
    Require(page->loadUserData == nullptr);

    auto prefs = NKWebKit::Make<NKWebKit::Preferences>();
    const void* prefsRef = NKWebKit::ToRef(prefs);

    RequireInvalidArgument([fileURLRef, allowedDirRef] { WKPageLoadFile(nullptr, fileURLRef, allowedDirRef); });
    RequireInvalidArgument([prefsRef, fileURLRef, allowedDirRef] { WKPageLoadFile(prefsRef, fileURLRef, allowedDirRef); });
    RequireInvalidArgument([pageRef, allowedDirRef] { WKPageLoadFile(pageRef, nullptr, allowedDirRef); });
    RequireInvalidArgument([pageRef, prefsRef, allowedDirRef] { WKPageLoadFile(pageRef, prefsRef, allowedDirRef); });
    RequireInvalidArgument([pageRef, fileURLRef, prefsRef] { WKPageLoadFile(pageRef, fileURLRef, prefsRef); });

    RequireInvalidArgument([fileURLRef, allowedDirRef, &userData] { WKPageLoadFileWithUserData(nullptr, fileURLRef, allowedDirRef, &userData); });
    RequireInvalidArgument([prefsRef, fileURLRef, allowedDirRef, &userData] { WKPageLoadFileWithUserData(prefsRef, fileURLRef, allowedDirRef, &userData); });
    RequireInvalidArgument([pageRef, allowedDirRef, &userData] { WKPageLoadFileWithUserData(pageRef, nullptr, allowedDirRef, &userData); });
    RequireInvalidArgument([pageRef, prefsRef, allowedDirRef, &userData] { WKPageLoadFileWithUserData(pageRef, prefsRef, allowedDirRef, &userData); });
    RequireInvalidArgument([pageRef, fileURLRef, prefsRef, &userData] { WKPageLoadFileWithUserData(pageRef, fileURLRef, prefsRef, &userData); });

    WKRelease(prefsRef);
    WKRelease(allowedDirRef);
    WKRelease(fileURLRef);
    WKRelease(pageRef);
    return 0;
}
