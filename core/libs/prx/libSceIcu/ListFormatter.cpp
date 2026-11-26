#include "prx/libSceIcu/ListFormatterBackend.hpp"
#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::mutex g_listFormattersMutex;
std::unordered_set<const void*> g_openListFormatters;
std::unordered_set<const void*> g_openFormattedLists;

void RegisterFormatter(const void* formatter) {
    if (!formatter) return;
    std::lock_guard lock(g_listFormattersMutex);
    g_openListFormatters.insert(formatter);
}

bool IsRegisteredFormatter(const void* formatter) {
    if (!formatter) return false;
    std::lock_guard lock(g_listFormattersMutex);
    return g_openListFormatters.contains(formatter);
}

bool UnregisterFormatter(const void* formatter) {
    if (!formatter) return false;
    std::lock_guard lock(g_listFormattersMutex);
    return g_openListFormatters.erase(formatter) > 0;
}

void RegisterResult(const void* result) {
    if (!result) return;
    std::lock_guard lock(g_listFormattersMutex);
    g_openFormattedLists.insert(result);
}

bool IsRegisteredResult(const void* result) {
    if (!result) return false;
    std::lock_guard lock(g_listFormattersMutex);
    return g_openFormattedLists.contains(result);
}

bool UnregisterResult(const void* result) {
    if (!result) return false;
    std::lock_guard lock(g_listFormattersMutex);
    return g_openFormattedLists.erase(result) > 0;
}

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    const int savedErrno = errno;
    std::int32_t value = 0;
    const bool ok = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
    return value;
}

void WriteStatus(std::int32_t* status, std::int32_t incoming, std::int32_t outgoing, const char* caller) {
    if (outgoing == incoming) return;
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(status, &outgoing, sizeof(outgoing));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

std::string ReadGuestCString(const char* source, const char* caller) {
    std::string text;
    for (;;) {
        char chunk[0x1000];
        const auto address = reinterpret_cast<std::uintptr_t>(source) + text.size();
        const std::size_t count = sizeof(chunk) - address % sizeof(chunk);
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(chunk, source + text.size(), count);
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(caller);
        const auto* end = static_cast<const char*>(std::memchr(chunk, 0, count));
        text.append(chunk, end ? static_cast<std::size_t>(end - chunk) : count);
        if (end) return text;
    }
}

std::u16string ReadGuestU16String(const char16_t* source, std::int32_t length, const char* caller) {
    std::u16string text;
    if (length >= 0) {
        text.resize(static_cast<std::size_t>(length));
        if (length > 0) {
            const int savedErrno = errno;
            const bool ok = GuestCopyIn_nid_no_patch(text.data(), source, static_cast<std::size_t>(length) * sizeof(char16_t));
            errno = savedErrno;
            if (!ok) NotImplemented_nid_no_patch(caller);
        }
        return text;
    }
    for (;;) {
        char16_t ch = 0;
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(&ch, source + text.size(), sizeof(ch));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(caller);
        if (ch == 0) return text;
        text.push_back(ch);
    }
}

struct HostStrings {
    std::vector<std::u16string> storage;
    std::vector<const char16_t*> pointers;
    std::vector<std::int32_t> lengths;
};

HostStrings ReadGuestStrings(const char16_t* const strings[], const std::int32_t* stringLengths,
                             std::int32_t stringCount, const char* caller) {
    HostStrings host;
    if (stringCount <= 0) return host;
    std::vector<const char16_t*> guestPtrs(static_cast<std::size_t>(stringCount));
    {
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(
            guestPtrs.data(), strings, static_cast<std::size_t>(stringCount) * sizeof(const char16_t*));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(caller);
    }
    if (stringLengths != nullptr) {
        host.lengths.resize(static_cast<std::size_t>(stringCount));
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(
            host.lengths.data(), stringLengths, static_cast<std::size_t>(stringCount) * sizeof(std::int32_t));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(caller);
    }
    host.storage.reserve(static_cast<std::size_t>(stringCount));
    host.pointers.reserve(static_cast<std::size_t>(stringCount));
    for (std::int32_t i = 0; i < stringCount; ++i) {
        const auto idx = static_cast<std::size_t>(i);
        const std::int32_t len = stringLengths ? host.lengths[idx] : -1;
        host.storage.push_back(ReadGuestU16String(guestPtrs[idx], len, caller));
        host.pointers.push_back(host.storage.back().c_str());
        if (stringLengths) host.lengths[idx] = static_cast<std::int32_t>(host.storage.back().size());
    }
    return host;
}

}

extern "C" {

void APS5_VABI ulistfmt_close_nid_postfix(UListFormatter* listfmt) {
    if (!listfmt) return;
    if (!UnregisterFormatter(listfmt)) NotImplemented_nid_no_patch(__func__);
    Icu::ListFormatterBackend::Close(listfmt);
}

void APS5_VABI ulistfmt_closeResult_nid_postfix(UFormattedList* result) {
    if (!result) return;
    if (!UnregisterResult(result)) NotImplemented_nid_no_patch(__func__);
    Icu::ListFormatterBackend::CloseResult(result);
}

std::int32_t APS5_VABI ulistfmt_format_nid_postfix(const UListFormatter* listfmt, const char16_t* const strings[],
                                                   const std::int32_t* stringLengths, std::int32_t stringCount,
                                                   char16_t* result, std::int32_t resultCapacity,
                                                   std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return -1;
    if ((result == nullptr) ? resultCapacity != 0 : resultCapacity < 0) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return -1;
    }
    if (stringCount < 0 || (strings == nullptr && stringCount > 0)) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return -1;
    }
    if (!listfmt || !IsRegisteredFormatter(listfmt)) throw std::invalid_argument("ulistfmt_format: invalid formatter");
    const HostStrings host = ReadGuestStrings(strings, stringLengths, stringCount, __func__);
    std::vector<char16_t> buffer(resultCapacity > 0 ? static_cast<std::size_t>(resultCapacity) : 0u);
    auto local = incoming;
    const std::int32_t length = Icu::ListFormatterBackend::Format(
        listfmt, host.pointers.empty() ? nullptr : host.pointers.data(),
        host.lengths.empty() ? nullptr : host.lengths.data(), stringCount,
        resultCapacity > 0 ? buffer.data() : nullptr, resultCapacity, &local);
    if (resultCapacity > 0 && length >= 0 && local != Icu::IllegalArgument) {
        const std::size_t count = (std::min)(static_cast<std::size_t>(resultCapacity),
                                             static_cast<std::size_t>(length) + (length < resultCapacity ? 1u : 0u));
        if (count > 0) {
            const int savedErrno = errno;
            const bool ok = GuestCopyOut_nid_no_patch(result, buffer.data(), count * sizeof(char16_t));
            errno = savedErrno;
            if (!ok) NotImplemented_nid_no_patch(__func__);
        }
    }
    WriteStatus(status, incoming, local, __func__);
    return length;
}

void APS5_VABI ulistfmt_formatStringsToResult_nid_postfix(const UListFormatter* listfmt, const char16_t* const strings[],
                                                          const std::int32_t* stringLengths, std::int32_t stringCount,
                                                          UFormattedList* uresult, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return;
    if (!uresult) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return;
    }
    if (stringCount < 0 || (strings == nullptr && stringCount > 0)) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return;
    }
    if (!listfmt || !IsRegisteredFormatter(listfmt) || !IsRegisteredResult(uresult)) {
        throw std::invalid_argument("ulistfmt_formatStringsToResult: invalid handle");
    }
    const HostStrings host = ReadGuestStrings(strings, stringLengths, stringCount, __func__);
    auto local = incoming;
    Icu::ListFormatterBackend::FormatStringsToResult(
        listfmt, host.pointers.empty() ? nullptr : host.pointers.data(),
        host.lengths.empty() ? nullptr : host.lengths.data(), stringCount, uresult, &local);
    WriteStatus(status, incoming, local, __func__);
}

UListFormatter* APS5_VABI ulistfmt_openForType_nid_postfix(const char* locale, UListFormatterType type,
                                                           UListFormatterWidth width, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    std::string hostLocale;
    const char* localePtr = locale ? (hostLocale = ReadGuestCString(locale, __func__), hostLocale.c_str()) : uloc_getDefault();
    auto local = incoming;
    void* formatter = Icu::ListFormatterBackend::OpenForType(localePtr, type, width, &local);
    WriteStatus(status, incoming, local, __func__);
    if (local <= 0 && formatter) RegisterFormatter(formatter);
    return static_cast<UListFormatter*>(formatter);
}

UFormattedList* APS5_VABI ulistfmt_openResult_nid_postfix(std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    auto local = incoming;
    void* result = Icu::ListFormatterBackend::OpenResult(&local);
    WriteStatus(status, incoming, local, __func__);
    if (local <= 0 && result) RegisterResult(result);
    return static_cast<UFormattedList*>(result);
}

const UFormattedValue* APS5_VABI ulistfmt_resultAsValue_nid_postfix(const UFormattedList* result, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    if (!result) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    if (!IsRegisteredResult(result)) throw std::invalid_argument("ulistfmt_resultAsValue: invalid result");
    auto local = incoming;
    const void* value = Icu::ListFormatterBackend::ResultAsValue(result, &local);
    WriteStatus(status, incoming, local, __func__);
    return static_cast<const UFormattedValue*>(value);
}

}
