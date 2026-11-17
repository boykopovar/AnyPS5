#include "prx/libSceIcu/Script.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uscript.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <new>
#include <string>

namespace {

UScriptCode NativeScript(std::int32_t script) {
    return script >= 0 && script < USCRIPT_CODE_LIMIT ? static_cast<UScriptCode>(script) : USCRIPT_INVALID_CODE;
}

}

extern "C" {

std::int32_t APS5_VABI uscript_getCode_nid_postfix(const char* nameOrAbbrOrLocale, std::int32_t* scripts,
                                                   std::int32_t capacity, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    std::int32_t incoming = 0;
    int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&incoming, error, sizeof(incoming));
    errno = savedErrno;
    if (!readable) NotImplemented_nid_no_patch(__func__);
    if (incoming > 0) return 0;
    if (capacity < 0 || (capacity > 0 && !scripts)) {
        const std::int32_t updated = U_ILLEGAL_ARGUMENT_ERROR;
        savedErrno = errno;
        const bool written = GuestCopyOut_nid_no_patch(error, &updated, sizeof(updated));
        errno = savedErrno;
        if (!written) NotImplemented_nid_no_patch(__func__);
        return 0;
    }

    std::string nameCopy;
    if (nameOrAbbrOrLocale) {
        try {
            savedErrno = errno;
            const int copied = CopyGuestString_nid_no_patch(nameOrAbbrOrLocale, 1024 * 1024, nameCopy);
            errno = savedErrno;
            if (copied != 0) NotImplemented_nid_no_patch(__func__);
        } catch (const std::bad_alloc&) {
            const std::int32_t updated = U_MEMORY_ALLOCATION_ERROR;
            savedErrno = errno;
            const bool written = GuestCopyOut_nid_no_patch(error, &updated, sizeof(updated));
            errno = savedErrno;
            if (!written) NotImplemented_nid_no_patch(__func__);
            return 0;
        }
    }

    std::array<UScriptCode, USCRIPT_CODE_LIMIT> nativeScripts{};
    auto status = static_cast<UErrorCode>(incoming);
    const auto nativeCapacity = std::min(capacity, static_cast<std::int32_t>(nativeScripts.size()));
    const auto result = ::uscript_getCode(nameOrAbbrOrLocale ? nameCopy.c_str() : nullptr,
                                          nativeCapacity > 0 ? nativeScripts.data() : nullptr,
                                          nativeCapacity, &status);
    const auto copyCount = std::min({result, capacity, static_cast<std::int32_t>(nativeScripts.size())});
    if (copyCount > 0) {
        std::array<std::int32_t, USCRIPT_CODE_LIMIT> output{};
        std::copy_n(nativeScripts.begin(), copyCount, output.begin());
        savedErrno = errno;
        const bool written = GuestCopyOut_nid_no_patch(scripts, output.data(),
                                                       static_cast<std::size_t>(copyCount) * sizeof(output[0]));
        errno = savedErrno;
        if (!written) NotImplemented_nid_no_patch(__func__);
    }
    const std::int32_t updated = status;
    savedErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(error, &updated, sizeof(updated));
    errno = savedErrno;
    if (!written) NotImplemented_nid_no_patch(__func__);
    return result;
}

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
