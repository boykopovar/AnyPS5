#include "prx/libSceIcu/DefaultLocaleName.hpp"
#include "prx/libSceIcu/include/LegacyExports.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uloc.h"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    if (!status) throw std::invalid_argument(std::string(caller) + ": invalid argument");
    std::int32_t value = 0;
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!readable) throw std::invalid_argument(std::string(caller) + ": unreadable status");
    return value;
}

void WriteStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (outgoing == incoming) return;
    const int savedErrno = errno;
    const bool writable = GuestCopyOut_nid_no_patch(status, &outgoing, sizeof(outgoing));
    errno = savedErrno;
    if (!writable) throw std::invalid_argument(std::string(caller) + ": unwritable status");
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

std::string ReadText(const char* source, const char* caller, const char* name) {
    std::string text;
    while (text.size() < ULOC_FULLNAME_CAPACITY) {
        char chunk[0x1000];
        const auto address = reinterpret_cast<std::uintptr_t>(source) + text.size();
        const std::size_t count = (std::min)(
            sizeof(chunk) - address % sizeof(chunk), ULOC_FULLNAME_CAPACITY - text.size());
        const auto* current = reinterpret_cast<const char*>(address);
        if (!CopyIn(chunk, current, count))
            throw std::invalid_argument(std::string(caller) + ": unreadable " + name);
        const auto* end = static_cast<const char*>(std::memchr(chunk, 0, count));
        text.append(chunk, end ? static_cast<std::size_t>(end - chunk) : count);
        if (end) return text;
    }
    throw std::invalid_argument(std::string(caller) + ": oversized " + name);
}

using DisplayFunction = std::int32_t (*)(const char*, const char*, char16_t*, std::int32_t, UErrorCode*);

std::int32_t Display(
    const char* locale,
    const char* displayLocale,
    char16_t* output,
    std::int32_t outputCapacity,
    std::int32_t* status,
    DisplayFunction display,
    const char* caller) {
    const auto incoming = ReadStatus(status, caller);
    if (incoming > 0) return 0;
    if (outputCapacity < 0 || (!output && outputCapacity > 0)) {
        auto local = static_cast<UErrorCode>(incoming);
        const auto length = display(
            Icu::DefaultLocaleName(), Icu::DefaultLocaleName(), nullptr, outputCapacity, &local);
        WriteStatus(status, incoming, local, caller);
        return length;
    }
    const std::string localeText = locale ? ReadText(locale, caller, "locale") : Icu::DefaultLocaleName();
    const std::string displayText = displayLocale ? ReadText(displayLocale, caller, "display locale") : Icu::DefaultLocaleName();
    auto local = static_cast<UErrorCode>(incoming);
    if (outputCapacity == 0) {
        const auto length = display(localeText.c_str(), displayText.c_str(), nullptr, outputCapacity, &local);
        WriteStatus(status, incoming, local, caller);
        return length;
    }
    auto preflight = static_cast<UErrorCode>(incoming);
    const auto required = display(localeText.c_str(), displayText.c_str(), nullptr, 0, &preflight);
    if (U_FAILURE(preflight) && preflight != U_BUFFER_OVERFLOW_ERROR) {
        WriteStatus(status, incoming, preflight, caller);
        return required;
    }
    const auto maximum = required >= 0 && required < (std::numeric_limits<std::int32_t>::max)()
        ? required + 1
        : outputCapacity;
    const auto hostCapacity = (std::min)(outputCapacity, maximum);
    std::vector<char16_t> host(static_cast<std::size_t>(hostCapacity));
    local = static_cast<UErrorCode>(incoming);
    const auto length = display(localeText.c_str(), displayText.c_str(), host.data(), hostCapacity, &local);
    if (U_SUCCESS(local) || local == U_BUFFER_OVERFLOW_ERROR) {
        const auto copied = length >= hostCapacity ? hostCapacity : (std::max)(length + 1, 0);
        if (copied > 0 && !CopyOut(output, host.data(), static_cast<std::size_t>(copied) * sizeof(char16_t)))
            throw std::invalid_argument(std::string(caller) + ": unwritable output");
    }
    WriteStatus(status, incoming, local, caller);
    return length;
}

}

extern "C" {

std::int32_t APS5_VABI uloc_getDisplayScript_nid_postfix(
    const char* locale,
    const char* displayLocale,
    char16_t* script,
    std::int32_t scriptCapacity,
    std::int32_t* status) {
    return Display(locale, displayLocale, script, scriptCapacity, status, uloc_getDisplayScript, __func__);
}

std::int32_t APS5_VABI uloc_getDisplayCountry_nid_postfix(
    const char* locale,
    const char* displayLocale,
    char16_t* country,
    std::int32_t countryCapacity,
    std::int32_t* status) {
    return Display(locale, displayLocale, country, countryCapacity, status, uloc_getDisplayCountry, __func__);
}

}
