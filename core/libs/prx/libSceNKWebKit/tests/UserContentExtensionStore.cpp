#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

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

    ExpectErr<std::invalid_argument>([&] { WKUserContentExtensionStoreCompile(nullptr, idRef, jsonRef, nullptr, nullptr); });
    ExpectErr<std::invalid_argument>([&] { WKUserContentExtensionStoreCompile(storeRef, nullptr, jsonRef, nullptr, nullptr); });
    ExpectErr<std::invalid_argument>([&] { WKUserContentExtensionStoreCompile(storeRef, idRef, nullptr, nullptr, nullptr); });
    ExpectErr<std::invalid_argument>([&] { WKUserContentExtensionStoreLookup(nullptr, idRef, nullptr, nullptr); });
    ExpectErr<std::invalid_argument>([&] { WKUserContentExtensionStoreLookup(storeRef, nullptr, nullptr, nullptr); });
    ExpectErr<std::invalid_argument>([&] { WKUserContentExtensionStoreCreate(storeRef); });

    WKRelease(defaultStoreRef);
    WKRelease(jsonRef);
    WKRelease(idRef);
    WKRelease(storeRef);
    WKRelease(pathRef);
    return 0;
}
