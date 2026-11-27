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

class PreservedErrno {
public:
    PreservedErrno() : saved(errno) {}
    ~PreservedErrno() { errno = saved; }

private:
    int saved;
};

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    PreservedErrno preserved;
    return GuestCopyIn_nid_no_patch(destination, source, bytes);
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    PreservedErrno preserved;
    return GuestCopyOut_nid_no_patch(destination, source, bytes);
}

void WriteStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (outgoing == incoming) return;
    if (!CopyOut(status, &outgoing, sizeof(outgoing)))
        throw std::invalid_argument(std::string(caller) + ": unwritable status");
}

std::string ReadGuestString(const char* source, const char* caller, const char* name) {
    std::string text;
    constexpr std::size_t PageBytes = 0x1000;
    const auto base = reinterpret_cast<std::uintptr_t>(source);
    const auto maxAddress = std::numeric_limits<std::uintptr_t>::max();
    for (;;) {
        if (text.size() > maxAddress - base)
            throw std::invalid_argument(std::string(caller) + ": unreadable " + name);
        const std::uintptr_t address = base + text.size();
        const std::size_t toPageEnd = PageBytes - (address % PageBytes);
        const std::size_t remainingAddressSpace = (maxAddress - address) + 1;
        const std::size_t count = remainingAddressSpace == 0 ? toPageEnd : std::min(toPageEnd, remainingAddressSpace);
        char chunk[PageBytes];
        if (!CopyIn(chunk, reinterpret_cast<const void*>(address), count))
            throw std::invalid_argument(std::string(caller) + ": unreadable " + name);
        const auto* terminator = static_cast<const char*>(std::memchr(chunk, 0, count));
        text.append(chunk, terminator ? static_cast<std::size_t>(terminator - chunk) : count);
        if (terminator) return text;
    }
}

bool IsCurrencyKeyword(const char* keyword) {
    if (!keyword) return false;
    static constexpr char Currency[] = "currency";
    for (std::size_t index = 0; index < sizeof(Currency); ++index) {
        char ch = keyword[index];
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        if (ch != Currency[index]) return false;
    }
    return true;
}

} // namespace

extern "C" {

std::int32_t APS5_VABI uloc_getDisplayKeywordValue_nid_postfix(
    const char* locale,
    const char* keyword,
    const char* displayLocale,
    char16_t* dest,
    std::int32_t destCapacity,
    std::int32_t* status) {
    if (!status) return 0;

    std::int32_t incoming = 0;
    if (!CopyIn(&incoming, status, sizeof(incoming)))
        throw std::invalid_argument(std::string(__func__) + ": unreadable status");
    if (U_FAILURE(static_cast<UErrorCode>(incoming))) return 0;

    if (destCapacity < 0 || (destCapacity > 0 && !dest)) {
        WriteStatus(status, incoming, static_cast<std::int32_t>(U_ILLEGAL_ARGUMENT_ERROR), __func__);
        return 0;
    }

    std::string localeStorage;
    const char* nativeLocale = nullptr;
    if (locale) {
        localeStorage = ReadGuestString(locale, __func__, "locale");
        nativeLocale = localeStorage.c_str();
    }

    std::string keywordStorage;
    const char* nativeKeyword = nullptr;
    if (keyword) {
        keywordStorage = ReadGuestString(keyword, __func__, "keyword");
        nativeKeyword = keywordStorage.c_str();
    }

    std::string displayLocaleStorage;
    const char* nativeDisplayLocale = nullptr;
    if (displayLocale) {
        displayLocaleStorage = ReadGuestString(displayLocale, __func__, "displayLocale");
        nativeDisplayLocale = displayLocaleStorage.c_str();
    } else {
        nativeDisplayLocale = Icu::DefaultLocaleName();
    }

    UErrorCode preflightStatus = static_cast<UErrorCode>(incoming);
    const std::int32_t preflightLength = uloc_getDisplayKeywordValue(
        nativeLocale, nativeKeyword, nativeDisplayLocale, nullptr, 0, &preflightStatus);

    if (destCapacity == 0 || (U_FAILURE(preflightStatus) && preflightStatus != U_BUFFER_OVERFLOW_ERROR)) {
        WriteStatus(status, incoming, static_cast<std::int32_t>(preflightStatus), __func__);
        return preflightLength;
    }

    const auto hostCapacity = static_cast<std::int32_t>(
        std::min<std::int64_t>(destCapacity, static_cast<std::int64_t>(preflightLength) + 1));
    std::vector<char16_t> buffer(static_cast<std::size_t>(hostCapacity));

    UErrorCode nativeStatus = static_cast<UErrorCode>(incoming);
    const std::int32_t length = uloc_getDisplayKeywordValue(
        nativeLocale, nativeKeyword, nativeDisplayLocale, buffer.data(), hostCapacity, &nativeStatus);

    std::size_t written = 0;
    if (U_SUCCESS(nativeStatus)) {
        written = length < hostCapacity
                      ? static_cast<std::size_t>(length) + 1
                      : static_cast<std::size_t>(hostCapacity);
    } else if (nativeStatus == U_BUFFER_OVERFLOW_ERROR && !IsCurrencyKeyword(nativeKeyword)) {
        written = static_cast<std::size_t>(hostCapacity);
    }

    if (written > 0 && !CopyOut(dest, buffer.data(), written * sizeof(char16_t)))
        throw std::invalid_argument(std::string(__func__) + ": unwritable dest");

    WriteStatus(status, incoming, static_cast<std::int32_t>(nativeStatus), __func__);
    return length;
}

}
