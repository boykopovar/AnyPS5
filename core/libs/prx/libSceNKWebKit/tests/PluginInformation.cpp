#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "PluginInformation test failed at line %d\n", line);
        std::abort();
    }
}
#define Require(cond) Check((cond), __LINE__)

struct KeyEntry {
    const void* (APS5_VABI *getter)();
    const char* expected;
};

const KeyEntry Entries[] = {
    {WKPlugInInformationReplacementObscuredKey, "ReplacementObscured"},
    {WKPluginInformationBundleIdentifierKey, "BundleIdentifier"},
    {WKPluginInformationBundleShortVersionKey, "BundleShortVersion"},
    {WKPluginInformationBundleVersionKey, "BundleVersion"},
    {WKPluginInformationDefaultLoadPolicyKey, "DefaultLoadPolicy"},
    {WKPluginInformationDisplayNameKey, "DisplayName"},
    {WKPluginInformationFrameURLKey, "FrameURL"},
    {WKPluginInformationHasSandboxProfileKey, "HasSandboxProfile"},
    {WKPluginInformationMIMETypeKey, "MIMEType"},
    {WKPluginInformationPageURLKey, "PageURL"},
    {WKPluginInformationPathKey, "Path"},
    {WKPluginInformationPluginURLKey, "PluginURL"},
    {WKPluginInformationPluginspageAttributeURLKey, "PluginspageAttributeURL"},
    {WKPluginInformationUpdatePastLastBlockedVersionIsKnownAvailableKey, "UpdatePastLastBlockedVersionIsKnownAvailable"},
};

void TestKeysNonNullAndTypeID() {
    for (const auto& entry : Entries) {
        const void* key = entry.getter();
        Require(key != nullptr);
        Require(WKGetTypeID(key) == WKStringGetTypeID());
        Require(WKStringIsEqualToUTF8CString(key, entry.expected));
        Require(WKStringGetLength(key) == std::strlen(entry.expected));
        Require(!WKStringIsEmpty(key));
        Require(WKStringIsEqualToUTF8CStringIgnoringCase(key, entry.expected));

        char buffer[128]{};
        const std::size_t written = WKStringGetUTF8CString(key, buffer, sizeof(buffer));
        Require(written > 0);
        Require(std::strcmp(buffer, entry.expected) == 0);

        WKRelease(key);
    }
}

void TestKeysAreDistinct() {
    constexpr std::size_t count = sizeof(Entries) / sizeof(Entries[0]);
    for (std::size_t i = 0; i < count; ++i) {
        const void* keyA = Entries[i].getter();
        for (std::size_t j = i + 1; j < count; ++j) {
            const void* keyB = Entries[j].getter();
            Require(!WKStringIsEqual(keyA, keyB));
            WKRelease(keyB);
        }
        WKRelease(keyA);
    }
}

void TestDictionaryIntegration() {
    void* dictionary = WKMutableDictionaryCreate();
    Require(dictionary != nullptr);

    for (const auto& entry : Entries) {
        const void* key = entry.getter();
        const void* value = WKStringCreateWithUTF8CString(entry.expected);
        Require(WKDictionarySetItem(dictionary, key, value));

        const void* fetched = WKDictionaryGetItemForKey(dictionary, key);
        Require(fetched != nullptr);
        Require(WKStringIsEqual(fetched, value));

        WKRelease(value);
        WKRelease(key);
    }

    Require(WKDictionaryGetSize(dictionary) == sizeof(Entries) / sizeof(Entries[0]));

    const void* keysArray = WKDictionaryCopyKeys(dictionary);
    Require(keysArray != nullptr);
    Require(WKArrayGetSize(keysArray) == sizeof(Entries) / sizeof(Entries[0]));
    WKRelease(keysArray);

    WKRelease(dictionary);
}

}

int main() {
    TestKeysNonNullAndTypeID();
    TestKeysAreDistinct();
    TestDictionaryIntegration();
    std::puts("PluginInformation tests passed");
    return 0;
}
