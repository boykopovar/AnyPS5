#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <string>
#include <type_traits>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebCore value helper check failed at line %u\n", location.line());
        std::abort();
    }
}

using ReduceTimeResolution = WTF::WallTime (APS5_VABI *)(WTF::WallTime);
using PlatformDefaultMediaSourceEnabled = bool (APS5_VABI *)();
using ScriptNameToCode = std::int32_t (APS5_VABI *)(const String&);
static_assert(std::is_same_v<decltype(&_ZN7WebCore22ResourceLoadStatistics20reduceTimeResolutionEN3WTF8WallTimeE), ReduceTimeResolution>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore12SettingsBase33platformDefaultMediaSourceEnabledEv), PlatformDefaultMediaSourceEnabled>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore16scriptNameToCodeERKN3WTF6StringE), ScriptNameToCode>);

constexpr std::int32_t InvalidCode = -1;
constexpr std::int32_t Common = 0;
constexpr std::int32_t Inherited = 1;
constexpr std::int32_t Arabic = 2;
constexpr std::int32_t Cyrillic = 8;
constexpr std::int32_t Han = 17;
constexpr std::int32_t Hangul = 18;
constexpr std::int32_t Latin = 25;
constexpr std::int32_t KatakanaOrHiragana = 54;
constexpr std::int32_t SimplifiedHan = 73;
constexpr std::int32_t UnwrittenLanguages = 102;
constexpr std::int32_t Unknown = 103;

double Reduce(double seconds) {
    return _ZN7WebCore22ResourceLoadStatistics20reduceTimeResolutionEN3WTF8WallTimeE(WTF::WallTime{seconds}).m_value;
}

void TestReduceTimeResolution() {
    Require(Reduce(12.0) == 10.0);
    Require(Reduce(15.0) == 15.0);
    Require(Reduce(-1.0) == -5.0);
    Require(Reduce(0.0) == 0.0);
    Require(Reduce(4.999) == 0.0);
    Require(Reduce(1700000003.75) == 1700000000.0);
    Require(Reduce(std::numeric_limits<double>::infinity()) == std::numeric_limits<double>::infinity());
    Require(std::isnan(Reduce(std::numeric_limits<double>::quiet_NaN())));
}

void TestPlatformDefaultMediaSourceEnabled() {
    Require(_ZN7WebCore12SettingsBase33platformDefaultMediaSourceEnabledEv());
}

std::int32_t Code8(const char* text) {
    StringImpl impl{4, static_cast<unsigned>(std::strlen(text)), text, 4};
    String name(&impl);
    std::int32_t code = _ZN7WebCore16scriptNameToCodeERKN3WTF6StringE(name);
    Require(impl.refCount == 4);
    name.impl = nullptr;
    return code;
}

std::int32_t Code16(const std::u16string& text) {
    StringImpl impl{4, static_cast<unsigned>(text.size()), text.data(), 0};
    String name(&impl);
    std::int32_t code = _ZN7WebCore16scriptNameToCodeERKN3WTF6StringE(name);
    name.impl = nullptr;
    return code;
}

void TestScriptNameToCode() {
    Require(Code8("arab") == Arabic);
    Require(Code8("ARAB") == Arabic);
    Require(Code8("Cyrl") == Cyrillic);
    Require(Code8("latn") == Latin);
    Require(Code8("hani") == Han);
    Require(Code8("hans") == SimplifiedHan);
    Require(Code8("kore") == Hangul);
    Require(Code8("Hira") == KatakanaOrHiragana);
    Require(Code8("jpan") == KatakanaOrHiragana);
    Require(Code8("qaai") == Inherited);
    Require(Code8("zyyy") == Common);
    Require(Code8("zxxx") == UnwrittenLanguages);
    Require(Code8("zzzz") == Unknown);
    Require(Code8("abc") == InvalidCode);
    Require(Code8("arabx") == InvalidCode);
    Require(Code8("") == InvalidCode);
    Require(Code8("ar@b") == InvalidCode);
    Require(Code8("zinh") == InvalidCode);
    Require(Code16(u"LaTn") == Latin);
    Require(Code16(u"\u0161rab") == InvalidCode);
    Require(Code16(std::u16string(u"arab\0", 5)) == InvalidCode);
    Require(Code16(std::u16string(u"ar\0b", 4)) == InvalidCode);
    String null;
    Require(_ZN7WebCore16scriptNameToCodeERKN3WTF6StringE(null) == InvalidCode);
}

}  // namespace

int main() {
    TestReduceTimeResolution();
    TestPlatformDefaultMediaSourceEnabled();
    TestScriptNameToCode();
    std::puts("WebCore value helpers passed");
    return 0;
}
