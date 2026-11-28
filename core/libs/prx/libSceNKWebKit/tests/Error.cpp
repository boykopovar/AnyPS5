#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKError check failed at line %u\n", location.line());
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

std::string CopyUtf8(const void* stringRef) {
    const std::size_t size = WKStringGetMaximumUTF8CStringSize(stringRef);
    std::string out(size, '\0');
    const std::size_t written = WKStringGetUTF8CString(stringRef, out.data(), size);
    if (written > 0) {
        out.resize(written - 1);
    } else {
        out.clear();
    }
    return out;
}

}

int main() {
    auto emptyError = NKWebKit::Make<NKWebKit::Error>();
    const void* emptyRef = emptyError.Get();

    Require(WKGetTypeID(emptyRef) == WKErrorGetTypeID());
    Require(WKErrorGetErrorCode(emptyRef) == 0);
    Require(WKErrorCopyFailingURL(emptyRef) == nullptr);

    const void* emptyDomain = WKErrorCopyDomain(emptyRef);
    Require(emptyDomain != nullptr && CopyUtf8(emptyDomain).empty());
    WKRelease(emptyDomain);

    const void* emptyDesc = WKErrorCopyLocalizedDescription(emptyRef);
    Require(emptyDesc != nullptr && CopyUtf8(emptyDesc).empty());
    WKRelease(emptyDesc);

    auto error = NKWebKit::Make<NKWebKit::Error>();
    error->domain = "WebKitErrorDomain";
    error->errorCode = 102;
    error->failingURL = "https://example.invalid/fail";
    error->localizedDescription = "Frame load interrupted";
    const void* errorRef = error.Get();

    const void* domain = WKErrorCopyDomain(errorRef);
    Require(domain != nullptr && CopyUtf8(domain) == "WebKitErrorDomain");
    WKRelease(domain);

    Require(WKErrorGetErrorCode(errorRef) == 102);

    const void* failingUrl = WKErrorCopyFailingURL(errorRef);
    Require(failingUrl != nullptr && WKGetTypeID(failingUrl) == WKURLGetTypeID());
    const void* failingUrlString = WKURLCopyString(failingUrl);
    Require(failingUrlString != nullptr && CopyUtf8(failingUrlString) == "https://example.invalid/fail");
    WKRelease(failingUrlString);
    WKRelease(failingUrl);

    const void* desc = WKErrorCopyLocalizedDescription(errorRef);
    Require(desc != nullptr && CopyUtf8(desc) == "Frame load interrupted");
    WKRelease(desc);

    RequireInvalidArgument([] { WKErrorCopyDomain(nullptr); });
    RequireInvalidArgument([] { WKErrorGetErrorCode(nullptr); });
    RequireInvalidArgument([] { WKErrorCopyFailingURL(nullptr); });
    RequireInvalidArgument([] { WKErrorCopyLocalizedDescription(nullptr); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    RequireInvalidArgument([&] { WKErrorCopyDomain(frame.Get()); });
    RequireInvalidArgument([&] { WKErrorGetErrorCode(frame.Get()); });
    RequireInvalidArgument([&] { WKErrorCopyFailingURL(frame.Get()); });
    RequireInvalidArgument([&] { WKErrorCopyLocalizedDescription(frame.Get()); });

    return 0;
}
