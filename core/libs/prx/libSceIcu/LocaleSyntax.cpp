#include "prx/libSceIcu/LocaleSyntax.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIcu/DefaultLocaleName.hpp"
#include "unicode/uloc.h"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

constexpr std::size_t Unbounded = static_cast<std::size_t>(-1);

std::string ReadText(const char* source, std::size_t limit, const char* caller, const char* name) {
    std::string text;
    while (text.size() < limit) {
        char chunk[0x1000];
        const auto address = reinterpret_cast<std::uintptr_t>(source + text.size());
        const std::size_t count = (std::min)(limit - text.size(), sizeof(chunk) - address % sizeof(chunk));
        if (!CopyIn(chunk, source + text.size(), count))
            throw std::invalid_argument(std::string(caller) + ": unreadable " + name);
        const auto* end = static_cast<const char*>(std::memchr(chunk, 0, count));
        text.append(chunk, end ? static_cast<std::size_t>(end - chunk) : count);
        if (end) break;
    }
    return text;
}

}

extern "C" {

std::int32_t APS5_VABI uloc_getVariant_nid_postfix(const char* locale, char* result,
                                                 std::int32_t capacity, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return 0;
    if (!locale) locale = Icu::DefaultLocaleName();
    if (!result && capacity > 0) APS5_INVALID_ARG_EX;
    auto local = static_cast<UErrorCode>(incoming);
    const auto length = uloc_getVariant(locale, result, capacity, &local);
    WriteStatus(status, incoming, local, __func__);
    return length;
}

std::int32_t APS5_VABI uloc_setKeywordValue_nid_postfix(const char* keyword, const char* value,
                                                      char* buffer, std::int32_t capacity,
                                                      std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return 0;
    const std::string keywordText = keyword ? ReadText(keyword, Unbounded, __func__, "keyword") : std::string();
    const bool named = keyword && !keywordText.empty();
    if (named && capacity > 1 && !buffer) APS5_INVALID_ARG_EX;
    auto local = static_cast<UErrorCode>(incoming);
    if (!named || capacity <= 1) {
        const auto length = uloc_setKeywordValue(keyword ? keywordText.c_str() : nullptr, nullptr, nullptr, capacity, &local);
        WriteStatus(status, incoming, local, __func__);
        return length;
    }
    const auto fullCapacity = static_cast<std::size_t>(capacity);
    const std::string initial = ReadText(buffer, fullCapacity + 1, __func__, "buffer");
    const bool valueRead = value && initial.size() <= fullCapacity;
    const std::string valueText = valueRead ? ReadText(value, Unbounded, __func__, "value") : std::string();
    auto hostCapacity = (std::min)(fullCapacity, initial.size() + 1);
    std::vector<char> host;
    std::int32_t length = 0;
    for (;;) {
        host.assign((std::max)(hostCapacity, initial.size() + 1), '\0');
        std::memcpy(host.data(), initial.data(), initial.size());
        local = static_cast<UErrorCode>(incoming);
        length = uloc_setKeywordValue(keywordText.c_str(), valueRead ? valueText.c_str() : nullptr, host.data(),
                                      static_cast<std::int32_t>(hostCapacity), &local);
        if (hostCapacity == fullCapacity || length < 0 || static_cast<std::size_t>(length) < hostCapacity) break;
        hostCapacity = (std::min)(fullCapacity, static_cast<std::size_t>(length) + 1);
    }
    if (U_SUCCESS(local) && length > 0) {
        const auto bytes = (std::min)(fullCapacity, static_cast<std::size_t>(length) + 1);
        if (!CopyOut(buffer, host.data(), bytes)) throw std::invalid_argument(std::string(__func__) + ": unwritable buffer");
    }
    WriteStatus(status, incoming, local, __func__);
    return length;
}

}
