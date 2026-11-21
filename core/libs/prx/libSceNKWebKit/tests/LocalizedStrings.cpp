#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "LocalizedStrings test failed at line %d\n", line);
        std::abort();
    }
}
#define Require(cond) Check((cond), __LINE__)

void RequireString(const NKWebKit::WtfAbi::String& str, std::string_view expected) {
    Require(str.impl != nullptr);
    Require((str.impl->hashAndFlags & 4) != 0);
    Require(str.impl->refCount == 2);
    Require(str.impl->length == expected.length());
    Require(std::memcmp(str.impl->data, expected.data(), expected.length()) == 0);
}

void RequireString16(const NKWebKit::WtfAbi::String& str, std::u16string_view expected) {
    Require(str.impl != nullptr);
    Require((str.impl->hashAndFlags & 4) == 0);
    Require(str.impl->refCount == 2);
    Require(str.impl->length == expected.length());
    Require(std::memcmp(str.impl->data, expected.data(), expected.length() * sizeof(char16_t)) == 0);
}

void TestLocalizedString() {
    RequireString(_ZN7WebCore15localizedStringEPKc("Choose File"), "Choose File");
    RequireString(_ZN7WebCore15localizedStringEPKc("%d files"), "%d files");
    RequireString16(_ZN7WebCore15localizedStringEPKc("caf\xC3\xA9"), u"café");
    RequireString16(_ZN7WebCore15localizedStringEPKc("\xE2\x98\xBA!"), u"☺!");

    auto empty = _ZN7WebCore15localizedStringEPKc("");
    Require(empty.impl != nullptr);
    Require(empty.impl->length == 0);

    auto invalid = _ZN7WebCore15localizedStringEPKc("bad\xFF");
    Require(invalid.impl == nullptr);

    const char key[] = "Submit\0Reset";
    RequireString(_ZN7WebCore15localizedStringEPKc(key), "Submit");
}

void TestMultipleFileUploadText() {
    RequireString(_ZN7WebCore22multipleFileUploadTextEj(0), "0 files");
    RequireString(_ZN7WebCore22multipleFileUploadTextEj(1), "1 files");
    RequireString(_ZN7WebCore22multipleFileUploadTextEj(2), "2 files");
    RequireString(_ZN7WebCore22multipleFileUploadTextEj(10), "10 files");
    RequireString(_ZN7WebCore22multipleFileUploadTextEj(1234567), "1234567 files");
    RequireString(_ZN7WebCore22multipleFileUploadTextEj(2147483647u), "2147483647 files");
    RequireString(_ZN7WebCore22multipleFileUploadTextEj(2147483648u), "-2147483648 files");
    RequireString(_ZN7WebCore22multipleFileUploadTextEj(4294967295u), "-1 files");
}

void TestFixedLabels() {
    RequireString(_ZN7WebCore18pluginTooSmallTextEv(), "Plug-In too small");
    RequireString(_ZN7WebCore32useBlockedPlugInContextMenuTitleEv(), "Show in blocked plug-in");

    auto first = _ZN7WebCore18pluginTooSmallTextEv();
    auto second = _ZN7WebCore18pluginTooSmallTextEv();
    Require(first.impl != second.impl);
    RequireString(first, "Plug-In too small");
    RequireString(second, "Plug-In too small");
}

}

int main() {
    TestLocalizedString();
    TestMultipleFileUploadText();
    TestFixedLabels();
    return 0;
}
