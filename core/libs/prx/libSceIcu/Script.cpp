#include "prx/libSceIcu/Script.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uscript.h"
#include <algorithm>
#include <array>

namespace {

UScriptCode NativeScript(std::int32_t script) {
    return script >= 0 && script < USCRIPT_CODE_LIMIT ? static_cast<UScriptCode>(script) : USCRIPT_INVALID_CODE;
}

}

extern "C" {

std::int32_t APS5_VABI uscript_getScript_nid_postfix(std::int32_t character, std::int32_t* error) {
    if (!error) return USCRIPT_INVALID_CODE;
    std::int32_t previous;
    if (!GuestCopyIn_nid_no_patch(&previous, error, sizeof(previous))) NotImplemented_nid_no_patch(__func__);
    if (previous > 0) return USCRIPT_INVALID_CODE;
    UErrorCode status = U_ZERO_ERROR;
    const auto result = uscript_getScript(character, &status);
    if (U_FAILURE(status)) {
        const std::int32_t updated = status;
        if (!GuestCopyOut_nid_no_patch(error, &updated, sizeof(updated))) NotImplemented_nid_no_patch(__func__);
    }
    return result;
}

std::int8_t APS5_VABI uscript_hasScript_nid_postfix(std::int32_t character, std::int32_t script) {
    return uscript_hasScript(character, NativeScript(script));
}

const char* APS5_VABI uscript_getShortName_nid_postfix(std::int32_t script) {
    return uscript_getShortName(NativeScript(script));
}

const char* APS5_VABI uscript_getName_nid_postfix(std::int32_t script) {
    return uscript_getName(NativeScript(script));
}

std::int32_t APS5_VABI uscript_getScriptExtensions_nid_postfix(std::int32_t character, std::int32_t* scripts,
                                                            std::int32_t capacity, std::int32_t* error) {
    if (!error) return 0;
    std::int32_t previous;
    if (!GuestCopyIn_nid_no_patch(&previous, error, sizeof(previous))) NotImplemented_nid_no_patch(__func__);
    if (previous > 0) return 0;
    UErrorCode status = U_ZERO_ERROR;
    std::array<UScriptCode, USCRIPT_CODE_LIMIT> nativeScripts{};
    const auto result = uscript_getScriptExtensions(character, scripts ? nativeScripts.data() : nullptr,
                                                    std::min(capacity, static_cast<std::int32_t>(nativeScripts.size())), &status);
    const auto count = std::min(result, std::max(capacity, 0));
    if (count > 0) {
        std::array<std::int32_t, USCRIPT_CODE_LIMIT> output{};
        std::copy_n(nativeScripts.begin(), count, output.begin());
        if (!GuestCopyOut_nid_no_patch(scripts, output.data(), count * sizeof(output[0]))) NotImplemented_nid_no_patch(__func__);
    }
    if (U_FAILURE(status)) {
        const std::int32_t updated = status;
        if (!GuestCopyOut_nid_no_patch(error, &updated, sizeof(updated))) NotImplemented_nid_no_patch(__func__);
    }
    return result;
}

std::int32_t APS5_VABI uscript_getUsage_nid_postfix(std::int32_t script) {
    return uscript_getUsage(NativeScript(script));
}

std::int8_t APS5_VABI uscript_isRightToLeft_nid_postfix(std::int32_t script) {
    return uscript_isRightToLeft(NativeScript(script));
}

std::int8_t APS5_VABI uscript_breaksBetweenLetters_nid_postfix(std::int32_t script) {
    return uscript_breaksBetweenLetters(NativeScript(script));
}

std::int8_t APS5_VABI uscript_isCased_nid_postfix(std::int32_t script) {
    return uscript_isCased(NativeScript(script));
}

std::int32_t APS5_VABI uscript_getSampleString_nid_postfix(std::int32_t script, char16_t* destination,
                                                        std::int32_t capacity, std::int32_t* error) {
    std::int32_t previous;
    if (!GuestCopyIn_nid_no_patch(&previous, error, sizeof(previous))) NotImplemented_nid_no_patch(__func__);
    if (previous > 0) return 0;
    UErrorCode status = previous == U_STRING_NOT_TERMINATED_WARNING ? U_STRING_NOT_TERMINATED_WARNING : U_ZERO_ERROR;
    std::array<char16_t, 3> output{};
    const auto result = uscript_getSampleString(NativeScript(script), destination ? output.data() : nullptr,
                                                std::min(capacity, static_cast<std::int32_t>(output.size())), &status);
    if (U_SUCCESS(status) && capacity > 0) {
        const auto count = std::min(result + 1, capacity);
        if (!GuestCopyOut_nid_no_patch(destination, output.data(), count * sizeof(output[0]))) NotImplemented_nid_no_patch(__func__);
    }
    const std::int32_t updated = status != U_ZERO_ERROR || previous == U_STRING_NOT_TERMINATED_WARNING ? status : previous;
    if (updated != previous && !GuestCopyOut_nid_no_patch(error, &updated, sizeof(updated))) NotImplemented_nid_no_patch(__func__);
    return result;
}

}
