#include "prx/libSceIcu/Script.hpp"

#include "unicode/uscript.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace {

constexpr auto nativeGetScript = &::uscript_getScript;
constexpr auto nativeGetShortName = &::uscript_getShortName;
constexpr auto nativeHasScript = &::uscript_hasScript;
constexpr auto nativeGetName = &::uscript_getName;
constexpr auto nativeGetScriptExtensions = &::uscript_getScriptExtensions;
constexpr auto nativeGetSampleString = &::uscript_getSampleString;
constexpr auto nativeGetUsage = &::uscript_getUsage;
constexpr auto nativeIsRightToLeft = &::uscript_isRightToLeft;
constexpr auto nativeBreaksBetweenLetters = &::uscript_breaksBetweenLetters;
constexpr auto nativeIsCased = &::uscript_isCased;

static_assert(sizeof(UScriptCode) == sizeof(std::int32_t));
static_assert(sizeof(UErrorCode) == sizeof(std::int32_t));
static_assert(sizeof(UBool) == sizeof(std::int8_t));
static_assert(sizeof(UChar) == sizeof(char16_t));

UErrorCode InputError(const std::int32_t* error) {
    return static_cast<UErrorCode>(*error);
}

void OutputError(std::int32_t* error, UErrorCode value) {
    *error = static_cast<std::int32_t>(value);
}

}

#undef uscript_getScript
#undef uscript_getShortName
#undef uscript_hasScript
#undef uscript_getName
#undef uscript_getScriptExtensions
#undef uscript_getSampleString
#undef uscript_getUsage
#undef uscript_isRightToLeft
#undef uscript_breaksBetweenLetters
#undef uscript_isCased

extern "C" {

std::int32_t APS5_VABI uscript_getScript(std::int32_t codePoint, std::int32_t* error) {
    if (!error) return -1;
    auto status = InputError(error);
    const auto script = nativeGetScript(codePoint, &status);
    OutputError(error, status);
    return static_cast<std::int32_t>(script);
}

const char* APS5_VABI uscript_getShortName(std::int32_t script) {
    return nativeGetShortName(static_cast<UScriptCode>(script));
}

std::int8_t APS5_VABI uscript_hasScript(std::int32_t codePoint, std::int32_t script) {
    return nativeHasScript(codePoint, static_cast<UScriptCode>(script));
}

const char* APS5_VABI uscript_getName_nid_postfix(std::int32_t script) {
    return nativeGetName(static_cast<UScriptCode>(script));
}

std::int32_t APS5_VABI uscript_getScriptExtensions_nid_postfix(std::int32_t codePoint,
    std::int32_t* scripts, std::int32_t capacity, std::int32_t* error) {
    if (!error) return 0;
    auto status = InputError(error);
    std::array<UScriptCode, 256> nativeScripts{};
    const std::int32_t available = scripts && capacity > 0
        ? std::min(capacity, static_cast<std::int32_t>(nativeScripts.size())) : capacity;
    auto* output = scripts && capacity > 0 ? nativeScripts.data() : nullptr;
    const auto result = nativeGetScriptExtensions(codePoint, output, available, &status);
    if (scripts && result > 0 && capacity > 0) {
        for (std::int32_t i = 0; i < std::min(result, available); ++i) {
            scripts[i] = static_cast<std::int32_t>(nativeScripts[i]);
        }
    }
    OutputError(error, status);
    return result;
}

std::int32_t APS5_VABI uscript_getSampleString_nid_postfix(std::int32_t script,
    char16_t* destination, std::int32_t capacity, std::int32_t* error) {
    if (!error) return 0;
    auto status = InputError(error);
    const auto length = nativeGetSampleString(static_cast<UScriptCode>(script), destination, capacity, &status);
    OutputError(error, status);
    return length;
}

std::int32_t APS5_VABI uscript_getUsage_nid_postfix(std::int32_t script) {
    return static_cast<std::int32_t>(nativeGetUsage(static_cast<UScriptCode>(script)));
}

std::int8_t APS5_VABI uscript_isRightToLeft_nid_postfix(std::int32_t script) {
    return nativeIsRightToLeft(static_cast<UScriptCode>(script));
}

std::int8_t APS5_VABI uscript_breaksBetweenLetters_nid_postfix(std::int32_t script) {
    return nativeBreaksBetweenLetters(static_cast<UScriptCode>(script));
}

std::int8_t APS5_VABI uscript_isCased_nid_postfix(std::int32_t script) {
    return nativeIsCased(static_cast<UScriptCode>(script));
}

}
