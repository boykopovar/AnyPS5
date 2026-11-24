#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"
#include "unicode/ucnv_cb.h"
#include "unicode/ucnv_err.h"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#undef ucnv_setFromUCallBack
#undef ucnv_setToUCallBack

namespace {

constexpr std::int32_t IllegalArgument = 1;

void ValidateReason(std::int32_t reason) {
    if (reason < 0 || reason > 7) APS5_INVALID_ARG_EX;
}

void RequireCallbackPointers(const void* arguments, std::int32_t reason, const void* error) {
    ValidateReason(reason);
    if (!arguments || !error) APS5_INVALID_ARG_EX;
}

void ClearError(std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    *error = 0;
}

bool IsDefaultIgnorable(std::int32_t codePoint) {
    constexpr std::array<std::array<std::int32_t, 2>, 17> ranges{{
        {0x00ad, 0x00ad}, {0x034f, 0x034f}, {0x061c, 0x061c},
        {0x115f, 0x1160}, {0x17b4, 0x17b5}, {0x180b, 0x180f},
        {0x200b, 0x200f}, {0x202a, 0x202e}, {0x2060, 0x206f},
        {0x3164, 0x3164}, {0xfe00, 0xfe0f}, {0xfeff, 0xfeff},
        {0xffa0, 0xffa0}, {0xfff0, 0xfff8}, {0x1bca0, 0x1bca3},
        {0x1d173, 0x1d17a}, {0xe0000, 0xe0fff}
    }};
    for (const auto& range : ranges)
        if (codePoint >= range[0] && codePoint <= range[1]) return true;
    return false;
}

bool ContextSkips(const void* context, std::int32_t reason) {
    return !context || (*static_cast<const char*>(context) == 'i' && reason == 0);
}

struct ConverterCallBack {
    UConverter* converter = nullptr;
    bool toUnicode = false;
    Icu::ConverterFromUCallback fromU = nullptr;
    const void* fromUContext = nullptr;
    Icu::ConverterToUCallback toU = nullptr;
    const void* toUContext = nullptr;
};

std::mutex converterCallBacksMutex;
std::vector<std::unique_ptr<ConverterCallBack>> converterCallBacks;

ConverterCallBack& CallBackFor(UConverter* converter, bool toUnicode) {
    std::lock_guard lock(converterCallBacksMutex);
    for (const auto& candidate : converterCallBacks)
        if (candidate->converter == converter && candidate->toUnicode == toUnicode) return *candidate;
    converterCallBacks.push_back(std::make_unique<ConverterCallBack>());
    auto& added = *converterCallBacks.back();
    added.converter = converter;
    added.toUnicode = toUnicode;
    return added;
}

struct GuestFromU {
    Icu::ConverterFromUCallback action = nullptr;
    const void* context = nullptr;
};

struct GuestToU {
    Icu::ConverterToUCallback action = nullptr;
    const void* context = nullptr;
};

GuestFromU ReadFromU(const ConverterCallBack* callBack) {
    std::lock_guard lock(converterCallBacksMutex);
    return {callBack->fromU, callBack->fromUContext};
}

GuestToU ReadToU(const ConverterCallBack* callBack) {
    std::lock_guard lock(converterCallBacksMutex);
    return {callBack->toU, callBack->toUContext};
}

void WriteFromU(ConverterCallBack* callBack, Icu::ConverterFromUCallback action, const void* context) {
    std::lock_guard lock(converterCallBacksMutex);
    callBack->fromU = action;
    callBack->fromUContext = context;
}

void WriteToU(ConverterCallBack* callBack, Icu::ConverterToUCallback action, const void* context) {
    std::lock_guard lock(converterCallBacksMutex);
    callBack->toU = action;
    callBack->toUContext = context;
}

void ClearFromU(UConverter* converter) {
    std::lock_guard lock(converterCallBacksMutex);
    for (const auto& candidate : converterCallBacks)
        if (candidate->converter == converter && !candidate->toUnicode) {
            candidate->fromU = nullptr;
            candidate->fromUContext = nullptr;
            return;
        }
}

void ClearToU(UConverter* converter) {
    std::lock_guard lock(converterCallBacksMutex);
    for (const auto& candidate : converterCallBacks)
        if (candidate->converter == converter && candidate->toUnicode) {
            candidate->toU = nullptr;
            candidate->toUContext = nullptr;
            return;
        }
}

void U_CALLCONV ForwardFromU(const void* context, UConverterFromUnicodeArgs* arguments, const UChar* codeUnits,
                             std::int32_t length, UChar32 codePoint, UConverterCallbackReason reason,
                             UErrorCode* error) {
    const auto* callBack = static_cast<const ConverterCallBack*>(context);
    if (!callBack) return;
    const GuestFromU guest = ReadFromU(callBack);
    if (!guest.action) return;
    guest.action(guest.context, reinterpret_cast<Icu::ConverterFromUnicodeArgs*>(arguments),
                 reinterpret_cast<const char16_t*>(codeUnits), length, static_cast<std::int32_t>(codePoint),
                 static_cast<std::int32_t>(reason), reinterpret_cast<std::int32_t*>(error));
}

void U_CALLCONV ForwardToU(const void* context, UConverterToUnicodeArgs* arguments, const char* codeUnits,
                           std::int32_t length, UConverterCallbackReason reason, UErrorCode* error) {
    const auto* callBack = static_cast<const ConverterCallBack*>(context);
    if (!callBack) return;
    const GuestToU guest = ReadToU(callBack);
    if (!guest.action) return;
    guest.action(guest.context, reinterpret_cast<Icu::ConverterToUnicodeArgs*>(arguments), codeUnits, length,
                 static_cast<std::int32_t>(reason), reinterpret_cast<std::int32_t*>(error));
}

std::int32_t ReadStatus(std::int32_t* error, const char* caller) {
    std::int32_t status = 0;
    const int savedErrno = errno;
    const bool read = GuestCopyIn_nid_no_patch(&status, error, sizeof(status));
    errno = savedErrno;
    if (!read) NotImplemented_nid_no_patch(caller);
    return status;
}

void WriteStatus(std::int32_t* error, std::int32_t status, std::int32_t previous, const char* caller) {
    if (status == previous) return;
    const int savedErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(error, &status, sizeof(status));
    errno = savedErrno;
    if (!written) NotImplemented_nid_no_patch(caller);
}

void WriteOutput(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!written) NotImplemented_nid_no_patch(caller);
}

void RequireWritable(void* destination, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool writable = GuestWritable_nid_no_patch(destination, bytes);
    errno = savedErrno;
    if (!writable) NotImplemented_nid_no_patch(caller);
}

GuestFromU TranslateFromU(UConverterFromUCallback action, const void* context) {
    constexpr std::array<UConverterFromUCallback, 4> hostDefaults{
        UCNV_FROM_U_CALLBACK_STOP, UCNV_FROM_U_CALLBACK_SKIP, UCNV_FROM_U_CALLBACK_SUBSTITUTE,
        UCNV_FROM_U_CALLBACK_ESCAPE};
    constexpr std::array<Icu::ConverterFromUCallback, 4> guestDefaults{
        UCNV_FROM_U_CALLBACK_STOP_nid_postfix, UCNV_FROM_U_CALLBACK_SKIP_nid_postfix,
        UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix, UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix};
    if (action == ForwardFromU) {
        std::lock_guard lock(converterCallBacksMutex);
        for (const auto& candidate : converterCallBacks)
            if (candidate.get() == context) return {candidate->fromU, candidate->fromUContext};
        return {nullptr, context};
    }
    for (std::size_t index = 0; index < hostDefaults.size(); ++index)
        if (action == hostDefaults[index]) return {guestDefaults[index], context};
    return {nullptr, context};
}

GuestToU TranslateToU(UConverterToUCallback action, const void* context) {
    constexpr std::array<UConverterToUCallback, 4> hostDefaults{
        UCNV_TO_U_CALLBACK_STOP, UCNV_TO_U_CALLBACK_SKIP, UCNV_TO_U_CALLBACK_SUBSTITUTE, UCNV_TO_U_CALLBACK_ESCAPE};
    constexpr std::array<Icu::ConverterToUCallback, 4> guestDefaults{
        UCNV_TO_U_CALLBACK_STOP_nid_postfix, UCNV_TO_U_CALLBACK_SKIP_nid_postfix,
        UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix, UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix};
    if (action == ForwardToU) {
        std::lock_guard lock(converterCallBacksMutex);
        for (const auto& candidate : converterCallBacks)
            if (candidate.get() == context) return {candidate->toU, candidate->toUContext};
        return {nullptr, context};
    }
    for (std::size_t index = 0; index < hostDefaults.size(); ++index)
        if (action == hostDefaults[index]) return {guestDefaults[index], context};
    return {nullptr, context};
}

}

extern "C" {

void APS5_VABI UCNV_FROM_U_CALLBACK_STOP_nid_postfix(const void*, Icu::ConverterFromUnicodeArgs*, const char16_t*, std::int32_t, std::int32_t codePoint, std::int32_t reason, std::int32_t* error) {
    ValidateReason(reason);
    if (reason == 0 && IsDefaultIgnorable(codePoint)) ClearError(error);
}

void APS5_VABI UCNV_FROM_U_CALLBACK_SKIP_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs*, const char16_t*, std::int32_t, std::int32_t codePoint, std::int32_t reason, std::int32_t* error) {
    ValidateReason(reason);
    if (reason <= 2 && ((reason == 0 && IsDefaultIgnorable(codePoint)) || ContextSkips(context, reason))) ClearError(error);
}

void APS5_VABI UCNV_TO_U_CALLBACK_STOP_nid_postfix(const void*, Icu::ConverterToUnicodeArgs*, const char*, std::int32_t, std::int32_t reason, std::int32_t*) {
    ValidateReason(reason);
}

void APS5_VABI UCNV_TO_U_CALLBACK_SKIP_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs*, const char*, std::int32_t, std::int32_t reason, std::int32_t* error) {
    ValidateReason(reason);
    if (reason <= 2 && ContextSkips(context, reason)) ClearError(error);
}

void APS5_VABI UCNV_FROM_U_CALLBACK_SUBSTITUTE_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* error) {
    RequireCallbackPointers(arguments, reason, error);
    UCNV_FROM_U_CALLBACK_SUBSTITUTE(context, reinterpret_cast<UConverterFromUnicodeArgs*>(arguments), reinterpret_cast<const UChar*>(codeUnits), length, codePoint, static_cast<UConverterCallbackReason>(reason), reinterpret_cast<UErrorCode*>(error));
}

void APS5_VABI UCNV_FROM_U_CALLBACK_ESCAPE_nid_postfix(const void* context, Icu::ConverterFromUnicodeArgs* arguments, const char16_t* codeUnits, std::int32_t length, std::int32_t codePoint, std::int32_t reason, std::int32_t* error) {
    RequireCallbackPointers(arguments, reason, error);
    if (!codeUnits && length > 0) APS5_INVALID_ARG_EX;
    UCNV_FROM_U_CALLBACK_ESCAPE(context, reinterpret_cast<UConverterFromUnicodeArgs*>(arguments), reinterpret_cast<const UChar*>(codeUnits), length, codePoint, static_cast<UConverterCallbackReason>(reason), reinterpret_cast<UErrorCode*>(error));
}

void APS5_VABI UCNV_TO_U_CALLBACK_SUBSTITUTE_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* error) {
    RequireCallbackPointers(arguments, reason, error);
    UCNV_TO_U_CALLBACK_SUBSTITUTE(context, reinterpret_cast<UConverterToUnicodeArgs*>(arguments), codeUnits, length, static_cast<UConverterCallbackReason>(reason), reinterpret_cast<UErrorCode*>(error));
}

void APS5_VABI UCNV_TO_U_CALLBACK_ESCAPE_nid_postfix(const void* context, Icu::ConverterToUnicodeArgs* arguments, const char* codeUnits, std::int32_t length, std::int32_t reason, std::int32_t* error) {
    RequireCallbackPointers(arguments, reason, error);
    if (!codeUnits && length > 0) APS5_INVALID_ARG_EX;
    UCNV_TO_U_CALLBACK_ESCAPE(context, reinterpret_cast<UConverterToUnicodeArgs*>(arguments), codeUnits, length, static_cast<UConverterCallbackReason>(reason), reinterpret_cast<UErrorCode*>(error));
}

void APS5_VABI ucnv_cbFromUWriteSub_nid_postfix(Icu::ConverterFromUnicodeArgs* arguments, std::int32_t offsetIndex, std::int32_t* error) {
    ucnv_cbFromUWriteSub(reinterpret_cast<UConverterFromUnicodeArgs*>(arguments), offsetIndex, reinterpret_cast<UErrorCode*>(error));
}

void APS5_VABI ucnv_setFromUCallBack(UConverter* converter, Icu::ConverterFromUCallback newAction, const void* newContext,
                                     Icu::ConverterFromUCallback* oldAction, const void** oldContext,
                                     std::int32_t* error) {
    if (!error) return;
    const std::int32_t incoming = ReadStatus(error, __func__);
    if (incoming > 0) return;
    if (!converter) {
        WriteStatus(error, IllegalArgument, incoming, __func__);
        return;
    }
    if (oldAction) RequireWritable(oldAction, sizeof(*oldAction), __func__);
    if (oldContext) RequireWritable(oldContext, sizeof(*oldContext), __func__);

    ConverterCallBack* callBack = nullptr;
    const void* installed = newContext;
    if (newAction) {
        callBack = &CallBackFor(converter, false);
        installed = callBack;
    }
    UConverterFromUCallback hostOldAction = nullptr;
    const void* hostOldContext = nullptr;
    auto status = static_cast<UErrorCode>(incoming);
    U_ICU_ENTRY_POINT_RENAME(ucnv_setFromUCallBack)(converter, newAction ? ForwardFromU : nullptr, installed,
                                                    &hostOldAction, &hostOldContext, &status);
    const GuestFromU previous = TranslateFromU(hostOldAction, hostOldContext);
    if (U_SUCCESS(status)) {
        if (callBack) WriteFromU(callBack, newAction, newContext);
        else ClearFromU(converter);
    }
    if (oldAction) WriteOutput(oldAction, &previous.action, sizeof(previous.action), __func__);
    if (oldContext) WriteOutput(oldContext, &previous.context, sizeof(previous.context), __func__);
    WriteStatus(error, static_cast<std::int32_t>(status), incoming, __func__);
}

void APS5_VABI ucnv_setToUCallBack(UConverter* converter, Icu::ConverterToUCallback newAction, const void* newContext,
                                   Icu::ConverterToUCallback* oldAction, const void** oldContext, std::int32_t* error) {
    if (!error) return;
    const std::int32_t incoming = ReadStatus(error, __func__);
    if (incoming > 0) return;
    if (!converter) {
        WriteStatus(error, IllegalArgument, incoming, __func__);
        return;
    }
    if (oldAction) RequireWritable(oldAction, sizeof(*oldAction), __func__);
    if (oldContext) RequireWritable(oldContext, sizeof(*oldContext), __func__);

    ConverterCallBack* callBack = nullptr;
    const void* installed = newContext;
    if (newAction) {
        callBack = &CallBackFor(converter, true);
        installed = callBack;
    }
    UConverterToUCallback hostOldAction = nullptr;
    const void* hostOldContext = nullptr;
    auto status = static_cast<UErrorCode>(incoming);
    U_ICU_ENTRY_POINT_RENAME(ucnv_setToUCallBack)(converter, newAction ? ForwardToU : nullptr, installed,
                                                  &hostOldAction, &hostOldContext, &status);
    const GuestToU previous = TranslateToU(hostOldAction, hostOldContext);
    if (U_SUCCESS(status)) {
        if (callBack) WriteToU(callBack, newAction, newContext);
        else ClearToU(converter);
    }
    if (oldAction) WriteOutput(oldAction, &previous.action, sizeof(previous.action), __func__);
    if (oldContext) WriteOutput(oldContext, &previous.context, sizeof(previous.context), __func__);
    WriteStatus(error, static_cast<std::int32_t>(status), incoming, __func__);
}

}
