#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "UserContentExtensionStore check failed at line %u\n", location.line());
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

const void* lastResultFilter = nullptr;
std::uint32_t lastResultStatus = 999;

void OnCompileOrLookup(const void* filterRef, std::uint32_t status, void* context) {
    (void)context;
    lastResultFilter = filterRef;
    lastResultStatus = status;
}

}

int main() {
    auto pathStr = NKWebKit::String::FromUtf8("/app0/content_rules");
    const void* pathRef = NKWebKit::ToRef(pathStr);
    const void* storeRef = WKUserContentExtensionStoreCreate(pathRef);
    Require(storeRef != nullptr);
    const void* defaultStoreRef = WKUserContentExtensionStoreCreate(nullptr);
    Require(defaultStoreRef != nullptr);

    auto idStr = NKWebKit::String::FromUtf8("my-filter");
    const void* idRef = NKWebKit::ToRef(idStr);

    auto jsonStr = NKWebKit::String::FromUtf8("[]");
    const void* jsonRef = NKWebKit::ToRef(jsonStr);

    WKUserContentExtensionStoreLookup(storeRef, idRef, nullptr, OnCompileOrLookup);
    Require(lastResultFilter == nullptr);
    Require(lastResultStatus == 1);

    auto badJsonStr = NKWebKit::String::FromUtf8("invalid json");
    const void* badJsonRef = NKWebKit::ToRef(badJsonStr);
    WKUserContentExtensionStoreCompile(storeRef, idRef, badJsonRef, nullptr, OnCompileOrLookup);
    Require(lastResultFilter == nullptr);
    Require(lastResultStatus == 3);
    WKRelease(badJsonRef);

    auto emptyJsonStr = NKWebKit::String::FromUtf8("   ");
    const void* emptyJsonRef = NKWebKit::ToRef(emptyJsonStr);
    WKUserContentExtensionStoreCompile(storeRef, idRef, emptyJsonRef, nullptr, OnCompileOrLookup);
    Require(lastResultFilter == nullptr);
    Require(lastResultStatus == 3);
    WKRelease(emptyJsonRef);

    WKUserContentExtensionStoreCompile(storeRef, idRef, jsonRef, nullptr, OnCompileOrLookup);
    Require(lastResultFilter != nullptr);
    Require(lastResultStatus == 0);

    const void* compiledFilter = lastResultFilter;
    WKUserContentExtensionStoreLookup(storeRef, idRef, nullptr, OnCompileOrLookup);
    Require(lastResultFilter == compiledFilter);
    Require(lastResultStatus == 0);

    WKUserContentExtensionStoreCompile(storeRef, idRef, jsonRef, nullptr, nullptr);
    WKUserContentExtensionStoreLookup(storeRef, idRef, nullptr, nullptr);

    RequireInvalidArgument([&] { WKUserContentExtensionStoreCompile(nullptr, idRef, jsonRef, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKUserContentExtensionStoreCompile(storeRef, nullptr, jsonRef, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKUserContentExtensionStoreCompile(storeRef, idRef, nullptr, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKUserContentExtensionStoreLookup(nullptr, idRef, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKUserContentExtensionStoreLookup(storeRef, nullptr, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKUserContentExtensionStoreCreate(storeRef); });

    WKRelease(defaultStoreRef);
    WKRelease(jsonRef);
    WKRelease(idRef);
    WKRelease(storeRef);
    WKRelease(pathRef);
    return 0;
}
