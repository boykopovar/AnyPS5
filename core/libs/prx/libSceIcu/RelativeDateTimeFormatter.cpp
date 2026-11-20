#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <vector>

extern "C" {
URelativeDateTimeFormatter* ureldatefmt_open(const char*, void*, std::int32_t, std::int32_t, std::int32_t*);
void ureldatefmt_close(URelativeDateTimeFormatter*);
std::int32_t ureldatefmt_format(const URelativeDateTimeFormatter*, double, std::int32_t, char16_t*, std::int32_t, std::int32_t*);
std::int32_t ureldatefmt_formatNumeric(const URelativeDateTimeFormatter*, double, std::int32_t, char16_t*, std::int32_t, std::int32_t*);
}

namespace {

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    const int savedErrno = errno;
    std::int32_t value = 0;
    const bool ok = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
    return value;
}

void WriteStatus(std::int32_t* status, std::int32_t previous, std::int32_t value, const char* caller) {
    if (previous == value) return;
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

bool Failure(std::int32_t* status, std::int32_t& value, const char* caller) {
    value = ReadStatus(status, caller);
    return value > 0;
}

using FormatFunction = std::int32_t (*)(const URelativeDateTimeFormatter*, double, std::int32_t, char16_t*, std::int32_t, std::int32_t*);

std::int32_t Format(const URelativeDateTimeFormatter* formatter, double offset, std::int32_t unit, char16_t* result,
                    std::int32_t resultCapacity, std::int32_t* status, FormatFunction format, const char* caller) {
    std::int32_t previous = 0;
    if (Failure(status, previous, caller)) return 0;
    if (result == nullptr ? resultCapacity != 0 : resultCapacity < 0) {
        WriteStatus(status, previous, Icu::IllegalArgument, caller);
        return 0;
    }

    std::int32_t preflightStatus = previous;
    const std::int32_t required = format(formatter, offset, unit, nullptr, 0, &preflightStatus);
    if (!result || resultCapacity == 0 || (preflightStatus > 0 && preflightStatus != Icu::BufferOverflow)) {
        WriteStatus(status, previous, preflightStatus, caller);
        return required;
    }
    if (required < 0) {
        WriteStatus(status, previous, preflightStatus, caller);
        return required;
    }

    const auto requiredWithTerminator = static_cast<std::int64_t>(required) + 1;
    const auto outputCapacity = static_cast<std::int32_t>((std::min)(static_cast<std::int64_t>(resultCapacity), requiredWithTerminator));
    std::vector<char16_t> output(static_cast<std::size_t>(outputCapacity));
    std::int32_t outputStatus = previous;
    const std::int32_t written = format(formatter, offset, unit, output.data(), outputCapacity, &outputStatus);
    const auto outputUnits = written < 0 ? std::size_t{0} :
        static_cast<std::size_t>(written < outputCapacity ? written + 1 : outputCapacity);
    if (outputUnits && !GuestCopyOut_nid_no_patch(result, output.data(), outputUnits * sizeof(char16_t)))
        NotImplemented_nid_no_patch(caller);
    WriteStatus(status, previous, outputStatus, caller);
    return written;
}

}

extern "C" {

void APS5_VABI ureldatefmt_close_nid_postfix(URelativeDateTimeFormatter* reldatefmt) {
    if (!reldatefmt) return;
    ::ureldatefmt_close(reldatefmt);
}

std::int32_t APS5_VABI ureldatefmt_format_nid_postfix(const URelativeDateTimeFormatter* formatter, double offset, std::int32_t unit, char16_t* result, std::int32_t resultCapacity, std::int32_t* status) {
    return Format(formatter, offset, unit, result, resultCapacity, status, ::ureldatefmt_format, __func__);
}

std::int32_t APS5_VABI ureldatefmt_formatNumeric_nid_postfix(const URelativeDateTimeFormatter* formatter, double offset, std::int32_t unit, char16_t* result, std::int32_t resultCapacity, std::int32_t* status) {
    return Format(formatter, offset, unit, result, resultCapacity, status, ::ureldatefmt_formatNumeric, __func__);
}

URelativeDateTimeFormatter* APS5_VABI ureldatefmt_open_nid_postfix(const char* locale, UNumberFormat* formatter, std::int32_t width, std::int32_t context, std::int32_t* status) {
    std::int32_t previous = 0;
    if (Failure(status, previous, __func__)) return nullptr;
    std::int32_t error = previous;
    auto* const result = ::ureldatefmt_open(locale, formatter, width, context, &error);
    try {
        WriteStatus(status, previous, error, __func__);
    } catch (...) {
        if (result) ::ureldatefmt_close(result);
        throw;
    }
    return result;
}

}
