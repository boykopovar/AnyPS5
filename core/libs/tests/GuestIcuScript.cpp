#include "prx/libSceIcu/Script.hpp"
#include "unicode/uscript.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace {

constexpr auto nativeGetScript = &::uscript_getScript;
constexpr auto nativeGetShortName = &::uscript_getShortName;
constexpr auto nativeGetName = &::uscript_getName;
constexpr auto nativeHasScript = &::uscript_hasScript;
constexpr auto nativeGetExtensions = &::uscript_getScriptExtensions;
constexpr auto nativeGetSample = &::uscript_getSampleString;
constexpr auto nativeGetUsage = &::uscript_getUsage;
constexpr auto nativeIsRtl = &::uscript_isRightToLeft;
constexpr auto nativeBreaks = &::uscript_breaksBetweenLetters;
constexpr auto nativeIsCased = &::uscript_isCased;

void Require(bool condition, const char* reason) {
    if (condition) return;
    std::fprintf(stderr, "guest ICU scripts: %s\n", reason);
    std::abort();
}

bool EqualStrings(const char* first, const char* second) {
    return first == nullptr ? second == nullptr : second != nullptr && std::strcmp(first, second) == 0;
}

}

#undef uscript_getScript
#undef uscript_getShortName
#undef uscript_getName
#undef uscript_hasScript
#undef uscript_getScriptExtensions
#undef uscript_getSampleString
#undef uscript_getUsage
#undef uscript_isRightToLeft
#undef uscript_breaksBetweenLetters
#undef uscript_isCased

int main() {
    static_assert(std::is_same_v<decltype(uscript_getScript(0, nullptr)), std::int32_t>);
    static_assert(std::is_same_v<decltype(uscript_hasScript(0, 0)), std::int8_t>);
    static_assert(std::is_same_v<decltype(uscript_getShortName(0)), const char*>);

    struct ScriptCase {
        std::int32_t codePoint;
        std::int32_t script;
    };
    constexpr std::array cases{
        ScriptCase{0x0041, 25}, ScriptCase{0x03b1, 14},
        ScriptCase{0x05d0, 19}, ScriptCase{0x0627, 2},
        ScriptCase{0x0301, 1}, ScriptCase{0x0030, 0},
        ScriptCase{0x1e4d0, 199}
    };
    for (const auto& entry : cases) {
        std::int32_t error = 0;
        Require(uscript_getScript(entry.codePoint, &error) == entry.script, "script property mismatch");
        Require(error == 0, "script property returned an error");
        error = 1;
        Require(uscript_getScript(entry.codePoint, &error) == -1 && error == 1,
                "preexisting failure was not preserved");
    }

    std::int32_t error = 0;
    Require(uscript_getScript(0x110000, &error) == -1 && error == 1,
            "out-of-range scalar was not rejected");
    error = 0;
    Require(uscript_getScript(-1, &error) == -1 && error == 1,
            "negative scalar was not rejected");

    Require(std::strcmp(uscript_getShortName(25), "Latn") == 0, "Latin short name");
    Require(std::strcmp(uscript_getName_nid_postfix(25), "Latin") == 0, "Latin long name");
    Require(std::strcmp(uscript_getShortName(14), "Grek") == 0, "Greek short name");
    Require(uscript_getShortName(-1) == nullptr && uscript_getName_nid_postfix(100000) == nullptr,
            "invalid script name must be null");

    Require(uscript_hasScript(0x0041, 25) == 1 && uscript_hasScript(0x0041, 14) == 0,
            "basic script membership");
    Require(uscript_hasScript(0x30fc, 20) == 1 && uscript_hasScript(0x30fc, 22) == 1 &&
            uscript_hasScript(0x30fc, 25) == 0, "script extension membership");
    Require(uscript_hasScript(0x0041, -1) == 0, "negative script membership");

    std::array<std::int32_t, 8> scripts{};
    error = 0;
    const auto count = uscript_getScriptExtensions_nid_postfix(0x30fc, scripts.data(),
        static_cast<std::int32_t>(scripts.size()), &error);
    Require(count == 2 && error == 0 && scripts[0] == 20 && scripts[1] == 22,
            "script extension enumeration");
    error = 0;
    Require(uscript_getScriptExtensions_nid_postfix(0x30fc, nullptr, 0, &error) == 2 && error == 15,
            "script extension preflight");
    error = 0;
    Require(uscript_getScriptExtensions_nid_postfix(0x30fc, scripts.data(), 1, &error) == 2 &&
            error == 15 && scripts[0] == 20, "script extension partial output");
    error = 0;
    Require(uscript_getScriptExtensions_nid_postfix(0x30fc, nullptr, -1, &error) == 0 && error == 1,
            "script extension negative capacity");
    error = 0;
    Require(uscript_getScriptExtensions_nid_postfix(0x30fc, nullptr, 2, &error) == 0 && error == 1,
            "script extension missing destination");

    std::array<char16_t, 4> sample{};
    error = 0;
    const auto sampleLength = uscript_getSampleString_nid_postfix(25, sample.data(),
        static_cast<std::int32_t>(sample.size()), &error);
    Require(sampleLength == 1 && error == 0 && sample[0] != 0 && sample[1] == 0,
            "Latin script sample");
    error = 0;
    Require(uscript_getSampleString_nid_postfix(25, nullptr, 0, &error) == 1 && error == 15,
            "sample preflight");
    error = 0;
    Require(uscript_getSampleString_nid_postfix(25, nullptr, 2, &error) == 0 && error == 1,
            "sample invalid destination");

    Require(uscript_getUsage_nid_postfix(25) == 5, "Latin recommended usage");
    Require(uscript_getUsage_nid_postfix(-1) == 0, "invalid script usage");
    Require(uscript_isRightToLeft_nid_postfix(2) == 1 &&
            uscript_isRightToLeft_nid_postfix(25) == 0, "right-to-left property");
    Require(uscript_breaksBetweenLetters_nid_postfix(17) == 1 &&
            uscript_breaksBetweenLetters_nid_postfix(25) == 0, "line-break property");
    Require(uscript_isCased_nid_postfix(25) == 1 &&
            uscript_isCased_nid_postfix(20) == 0, "casing property");

    for (std::int32_t script = -2; script <= 215; ++script) {
        const auto nativeScript = static_cast<UScriptCode>(script);
        Require(EqualStrings(uscript_getShortName(script), nativeGetShortName(nativeScript)),
                "native script short-name differential");
        Require(EqualStrings(uscript_getName_nid_postfix(script), nativeGetName(nativeScript)),
                "native script long-name differential");
        Require(uscript_getUsage_nid_postfix(script) == static_cast<std::int32_t>(nativeGetUsage(nativeScript)),
                "native script usage differential");
        Require(uscript_isRightToLeft_nid_postfix(script) == nativeIsRtl(nativeScript),
                "native script direction differential");
        Require(uscript_breaksBetweenLetters_nid_postfix(script) == nativeBreaks(nativeScript),
                "native script line-break differential");
        Require(uscript_isCased_nid_postfix(script) == nativeIsCased(nativeScript),
                "native script casing differential");
        std::array<char16_t, 4> guestSample{};
        std::array<char16_t, 4> nativeSample{};
        std::int32_t guestError = 0;
        UErrorCode nativeError = U_ZERO_ERROR;
        const auto guestLength = uscript_getSampleString_nid_postfix(script, guestSample.data(), 4, &guestError);
        const auto nativeLength = nativeGetSample(nativeScript, nativeSample.data(), 4, &nativeError);
        Require(guestLength == nativeLength && guestError == static_cast<std::int32_t>(nativeError) &&
                guestSample == nativeSample, "native script sample differential");
    }

    for (std::int32_t codePoint = -1; codePoint <= 0x110000; codePoint += 37) {
        std::int32_t guestError = 0;
        UErrorCode nativeError = U_ZERO_ERROR;
        const auto guestScript = uscript_getScript(codePoint, &guestError);
        const auto nativeScript = nativeGetScript(codePoint, &nativeError);
        Require(guestScript == static_cast<std::int32_t>(nativeScript) &&
                guestError == static_cast<std::int32_t>(nativeError), "native code point script differential");
        for (const std::int32_t script : {0, 1, 2, 14, 17, 19, 20, 22, 25, 103, 199, 211}) {
            Require(uscript_hasScript(codePoint, script) == nativeHasScript(codePoint, static_cast<UScriptCode>(script)),
                    "native script membership differential");
        }
        std::array<std::int32_t, 256> guestScripts{};
        std::array<UScriptCode, 256> nativeScripts{};
        guestError = 0;
        nativeError = U_ZERO_ERROR;
        const auto guestCount = uscript_getScriptExtensions_nid_postfix(codePoint, guestScripts.data(), 256, &guestError);
        const auto nativeCount = nativeGetExtensions(codePoint, nativeScripts.data(), 256, &nativeError);
        Require(guestCount == nativeCount && guestError == static_cast<std::int32_t>(nativeError),
                "native script extension count differential");
        for (std::int32_t index = 0; index < guestCount; ++index) {
            Require(guestScripts[index] == static_cast<std::int32_t>(nativeScripts[index]),
                    "native script extension value differential");
        }
    }

    std::puts("guest ICU script tests passed");
}
