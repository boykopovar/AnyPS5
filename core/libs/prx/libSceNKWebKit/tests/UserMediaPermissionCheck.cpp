#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKUserMediaPermissionCheck check failed at line %u\n", location.line());
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
    Require(WKUserMediaPermissionCheckGetTypeID() == 102);

    auto check1 = NKWebKit::Make<NKWebKit::UserMediaPermissionCheck>();
    const void* ref1 = NKWebKit::ToRef(check1);
    Require(!check1->completed);
    Require(!check1->allowed);
    Require(!check1->userMediaID);

    auto idStr = NKWebKit::String::FromUtf8("audio-input-device-1");
    const void* idRef = NKWebKit::ToRef(idStr);

    WKUserMediaPermissionCheckSetUserMediaAccessInfo(ref1, idRef, true);
    Require(check1->completed);
    Require(check1->allowed);
    Require(static_cast<bool>(check1->userMediaID));
    Require(check1->userMediaID->Utf8() == "audio-input-device-1");

    auto check2 = NKWebKit::Make<NKWebKit::UserMediaPermissionCheck>();
    const void* ref2 = NKWebKit::ToRef(check2);
    WKUserMediaPermissionCheckSetUserMediaAccessInfo(ref2, nullptr, false);
    Require(check2->completed);
    Require(!check2->allowed);
    Require(!check2->userMediaID);

    auto invalidObj = NKWebKit::Make<NKWebKit::UserMediaPermissionRequest>();
    const void* invalidRef = NKWebKit::ToRef(invalidObj);

    RequireInvalidArgument([&] { WKUserMediaPermissionCheckSetUserMediaAccessInfo(nullptr, idRef, true); });
    RequireInvalidArgument([&] { WKUserMediaPermissionCheckSetUserMediaAccessInfo(invalidRef, idRef, true); });
    RequireInvalidArgument([&] { WKUserMediaPermissionCheckSetUserMediaAccessInfo(ref1, invalidRef, true); });

    WKRelease(ref1);
    WKRelease(ref2);
    WKRelease(idRef);
    WKRelease(invalidRef);
    return 0;
}
