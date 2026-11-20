#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore user agent check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr char kStandard[] = "Mozilla/5.0 (PlayStation; PlayStation/0.00) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.0 Safari/605.1.15";

static String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

static String Utf16(const std::u16string& text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

static bool Is8Bit(const String& string) {
    return string.impl->hashAndFlags & 4;
}

static bool Equals8(const String& string, const std::string& expected) {
    return string.impl && Is8Bit(string) && string.impl->length == expected.size()
        && std::memcmp(string.impl->data, expected.data(), expected.size()) == 0;
}

static bool Equals16(const String& string, const std::u16string& expected) {
    return string.impl && !Is8Bit(string) && string.impl->length == expected.size()
        && std::memcmp(string.impl->data, expected.data(), expected.size() * sizeof(char16_t)) == 0;
}

template<typename Function>
static bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String UserAgent(const String& name, const String& version) {
    return _ZN7WebCore17standardUserAgentERKN3WTF6StringES3_(&name, &version);
}

static void TestStandardUserAgent() {
    const String null;
    const String empty = Latin1("");
    Require(Equals8(UserAgent(null, null), kStandard));
    Require(Equals8(UserAgent(empty, Latin1("1.0")), kStandard));
    Require(UserAgent(null, null).impl == UserAgent(empty, empty).impl);
    Require(Equals8(UserAgent(Latin1("App"), Latin1("2.5")), std::string(kStandard) + " App/2.5"));
    Require(Equals8(UserAgent(Latin1("App"), null), std::string(kStandard) + " App/605.1.15"));
    Require(Equals8(UserAgent(Latin1("App"), empty), std::string(kStandard) + " App/605.1.15"));
    Require(Equals16(UserAgent(Utf16(u"アプリ"), Latin1("1")), std::u16string(std::begin(kStandard), std::end(kStandard) - 1) + u" アプリ/1"));
    Require(Throws([] { _ZN7WebCore17standardUserAgentERKN3WTF6StringES3_(nullptr, nullptr); }));
}

static void TestStandardUserAgentForURL() {
    NKWebKit::WtfAbi::URL url{};
    const String forUrl = _ZN7WebCore23standardUserAgentForURLERKN3WTF3URLE(&url);
    Require(Equals8(forUrl, kStandard));
    Require(forUrl.impl == UserAgent(String(), String()).impl);
}

int main() {
    TestStandardUserAgent();
    TestStandardUserAgentForURL();
    return 0;
}
