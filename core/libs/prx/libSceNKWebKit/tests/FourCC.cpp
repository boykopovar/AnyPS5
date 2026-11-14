#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using NKWebKit::WebCoreAbi::OptionalFourCC;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Require(bool value) {
    if (!value) {
        std::fprintf(stderr, "FourCC result or hidden-return ABI mismatch\n");
        std::abort();
    }
}

static void Check(const String& string, bool engaged, std::uint32_t value) {
    struct {
        std::uint32_t before = 0x12345678;
        OptionalFourCC result;
        std::uint32_t after = 0x87654321;
    } storage;
    std::memset(&storage.result, 0xa5, sizeof(storage.result));
    auto* returned = _ZN7WebCore6FourCC10fromStringERKN3WTF6StringE(&storage.result, &string);
    Require(returned == &storage.result);
    Require(storage.before == 0x12345678 && storage.after == 0x87654321);
    const auto* bytes = reinterpret_cast<const unsigned char*>(&storage.result);
    Require(bytes[0] == static_cast<unsigned char>(engaged));
    if (engaged) {
        std::uint32_t actual;
        std::memcpy(&actual, bytes + 4, sizeof(actual));
        Require(actual == value);
    }
}

static void Test(const std::u16string& text, bool engaged, std::uint32_t value) {
    for (bool wide : {false, true}) {
        std::string narrow;
        for (auto ch : text) narrow.push_back(static_cast<char>(ch));
        StringImpl impl{4, static_cast<unsigned>(text.size()), wide ? static_cast<const void*>(text.data()) : narrow.data(), wide ? 0u : 4u};
        String string(&impl);
        Check(string, engaged, value);
        string.impl = nullptr;
    }
}

int main() {
    Test(u"avc1", true, 0x61766331);
    Test(u"", false, 0);
    Test(u"abc", false, 0);
    Test(u"abcde", false, 0);
    Test(std::u16string{0, 0x7f, 1, 0x1f}, true, 0x007f3f3f);
    Test(std::u16string{0x80, 0xff, 0x20, 0x7e}, true, 0x3f3f207e);
    for (char16_t ch = 0; ch < 256; ++ch) {
        const std::uint32_t byte = ch && (ch < 32 || ch > 127) ? '?' : ch;
        Test(std::u16string{u'a', ch, u'c', u'1'}, true, 0x61006331 | (byte << 16));
    }
    const char16_t unicode[] = {0x100, 0x20ac, 0xd800, 0xdc00};
    StringImpl impl{4, 4, unicode, 0};
    String wide(&impl);
    Check(wide, true, 0x3f3f3f3f);
    wide.impl = nullptr;
    String nullString;
    Check(nullString, false, 0);
    return 0;
}
