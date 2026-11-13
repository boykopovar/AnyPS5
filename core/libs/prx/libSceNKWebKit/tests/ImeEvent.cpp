#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

struct ImeEventLayout {
    int imeEventType;
    const char* compositionString;
    int keyCode;
    int caretIndex;
};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ImeEvent check failed at line %u\n", location.line());
        std::abort();
    }
}

}

int main() {
    Require(WKImeEventGetInputLanguage(nullptr) == static_cast<WKInputLanguage>(-1));
    Require(WKImeEventGetInputLanguage("") == static_cast<WKInputLanguage>(-1));
    Require(WKImeEventGetInputLanguage("unknown_XYZ") == static_cast<WKInputLanguage>(-1));

    Require(WKImeEventGetInputLanguage("en_Latn_US") == static_cast<WKInputLanguage>(2));
    Require(WKImeEventGetInputLanguage("en") == static_cast<WKInputLanguage>(2));
    Require(WKImeEventGetInputLanguage("en-GB") == static_cast<WKInputLanguage>(18));
    Require(WKImeEventGetInputLanguage("ja_Jpan_JP") == static_cast<WKInputLanguage>(13));
    Require(WKImeEventGetInputLanguage("ja-JP") == static_cast<WKInputLanguage>(13));
    Require(WKImeEventGetInputLanguage("es_Latn_ES") == static_cast<WKInputLanguage>(3));
    Require(WKImeEventGetInputLanguage("es-419") == static_cast<WKInputLanguage>(20));
    Require(WKImeEventGetInputLanguage("de-DE") == static_cast<WKInputLanguage>(1));
    Require(WKImeEventGetInputLanguage("fr_CA") == static_cast<WKInputLanguage>(22));
    Require(WKImeEventGetInputLanguage("pt") == static_cast<WKInputLanguage>(17));
    Require(WKImeEventGetInputLanguage("pt_BR") == static_cast<WKInputLanguage>(17));
    Require(WKImeEventGetInputLanguage("pt_PT") == static_cast<WKInputLanguage>(9));
    Require(WKImeEventGetInputLanguage("zh") == static_cast<WKInputLanguage>(15));
    Require(WKImeEventGetInputLanguage("zh-CN") == static_cast<WKInputLanguage>(15));
    Require(WKImeEventGetInputLanguage("zh-Hans") == static_cast<WKInputLanguage>(15));
    Require(WKImeEventGetInputLanguage("zh_TW") == static_cast<WKInputLanguage>(16));
    Require(WKImeEventGetInputLanguage("zh-Hant") == static_cast<WKInputLanguage>(16));
    Require(WKImeEventGetInputLanguage("da") == static_cast<WKInputLanguage>(0));

    auto* event = WKImeEventMake(static_cast<WKImeEventType>(6), "hello", 42);
    Require(event != nullptr);
    ImeEventLayout layout{};
    std::memcpy(&layout, event, sizeof(layout));
    Require(layout.imeEventType == 6);
    Require(layout.keyCode == 42);
    Require(layout.caretIndex == 5);
    WKImeEventRelease(event);

    auto* multibyteEvent = WKImeEventMake(static_cast<WKImeEventType>(6), "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86", 7);
    Require(multibyteEvent != nullptr);
    std::memcpy(&layout, multibyteEvent, sizeof(layout));
    Require(layout.caretIndex == 3);
    WKImeEventRelease(multibyteEvent);

    auto* mixedEvent = WKImeEventMake(static_cast<WKImeEventType>(4), "\xED\x95\x9C\xEA\xB8\x80" "ab", 9);
    Require(mixedEvent != nullptr);
    std::memcpy(&layout, mixedEvent, sizeof(layout));
    Require(layout.caretIndex == 4);
    WKImeEventRelease(mixedEvent);

    auto* nullTextEvent = WKImeEventMake(static_cast<WKImeEventType>(0), nullptr, 0);
    Require(nullTextEvent != nullptr);
    std::memcpy(&layout, nullTextEvent, sizeof(layout));
    Require(layout.caretIndex == 0);
    WKImeEventRelease(nullTextEvent);
    return 0;
}
