#include "prx/libSceIcu/FieldPositionIteratorRegistry.hpp"
#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/NumberFormatterBackend.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::mutex g_numberFormatterMutex;
std::unordered_set<const void*> g_openFormatters;
std::unordered_set<const void*> g_openResults;

void RegisterFormatter(const void* formatter) {
    if (!formatter) return;
    std::lock_guard lock(g_numberFormatterMutex);
    g_openFormatters.insert(formatter);
}

bool IsRegisteredFormatter(const void* formatter) {
    if (!formatter) return false;
    std::lock_guard lock(g_numberFormatterMutex);
    return g_openFormatters.contains(formatter);
}

bool UnregisterFormatter(const void* formatter) {
    if (!formatter) return false;
    std::lock_guard lock(g_numberFormatterMutex);
    return g_openFormatters.erase(formatter) > 0;
}

void RegisterResult(const void* result) {
    if (!result) return;
    std::lock_guard lock(g_numberFormatterMutex);
    g_openResults.insert(result);
}

bool IsRegisteredResult(const void* result) {
    if (!result) return false;
    std::lock_guard lock(g_numberFormatterMutex);
    return g_openResults.contains(result);
}

bool UnregisterResult(const void* result) {
    if (!result) return false;
    std::lock_guard lock(g_numberFormatterMutex);
    return g_openResults.erase(result) > 0;
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

std::string ReadGuestBytes(const char* source, std::int32_t length, const char* caller) {
    if (length == -1) return ReadGuestCString(source, caller);
    std::string text(static_cast<std::size_t>(length), '\0');
    if (length > 0) {
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(text.data(), source, static_cast<std::size_t>(length));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(caller);
    }
    return text;
}

std::u16string ReadGuestU16(const char16_t* source, std::int32_t length, const char* caller) {
    if (length >= 0) {
        std::u16string text(static_cast<std::size_t>(length), u'\0');
        if (length > 0) {
            const int savedErrno = errno;
            const bool ok = GuestCopyIn_nid_no_patch(
                text.data(), source, static_cast<std::size_t>(length) * sizeof(char16_t));
            errno = savedErrno;
            if (!ok) NotImplemented_nid_no_patch(caller);
        }
        return text;
    }
    std::u16string text;
    for (std::size_t index = 0;; ++index) {
        char16_t ch = 0;
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(&ch, source + index, sizeof(ch));
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(caller);
        if (ch == 0) return text;
        text.push_back(ch);
    }
}

void WriteGuestU16Result(char16_t* destination, std::int32_t capacity, const std::vector<char16_t>& buffer,
                         std::int32_t length, std::int32_t outgoing, const char* caller) {
    if (capacity > 0 && length >= 0 && outgoing != Icu::IllegalArgument) {
        const std::size_t count = (std::min)(static_cast<std::size_t>(capacity),
                                             static_cast<std::size_t>(length) + (length < capacity ? 1u : 0u));
        if (count > 0) {
            const int savedErrno = errno;
            const bool ok = GuestCopyOut_nid_no_patch(destination, buffer.data(), count * sizeof(char16_t));
            errno = savedErrno;
            if (!ok) NotImplemented_nid_no_patch(caller);
        }
    }
}

}

extern "C" {

void APS5_VABI unumf_close_nid_postfix(UNumberFormatter* formatter) {
    if (!formatter) return;
    if (!UnregisterFormatter(formatter)) NotImplemented_nid_no_patch(__func__);
    Icu::NumberFormatterBackend::Close(formatter);
}

void APS5_VABI unumf_closeResult_nid_postfix(UFormattedNumber* result) {
    if (!result) return;
    if (!UnregisterResult(result)) NotImplemented_nid_no_patch(__func__);
    Icu::NumberFormatterBackend::CloseResult(result);
}

void APS5_VABI unumf_formatDecimal_nid_postfix(const UNumberFormatter* formatter, const char* value,
                                                 std::int32_t valueLen, UFormattedNumber* result,
                                                 std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return;
    if (!formatter) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return;
    }
    if (!IsRegisteredFormatter(formatter)) NotImplemented_nid_no_patch(__func__);
    if (!result || !value || valueLen < -1) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return;
    }
    if (!IsRegisteredResult(result)) NotImplemented_nid_no_patch(__func__);
    const std::string hostValue = ReadGuestBytes(value, valueLen, __func__);
    auto local = incoming;
    Icu::NumberFormatterBackend::FormatDecimal(
        formatter, hostValue.c_str(), static_cast<std::int32_t>(hostValue.size()), result, &local);
    WriteStatus(status, incoming, local, __func__);
}

void APS5_VABI unumf_formatDouble_nid_postfix(const UNumberFormatter* formatter, double value,
                                                UFormattedNumber* result, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return;
    if (!formatter) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return;
    }
    if (!IsRegisteredFormatter(formatter)) NotImplemented_nid_no_patch(__func__);
    if (!result) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return;
    }
    if (!IsRegisteredResult(result)) NotImplemented_nid_no_patch(__func__);
    auto local = incoming;
    Icu::NumberFormatterBackend::FormatDouble(formatter, value, result, &local);
    WriteStatus(status, incoming, local, __func__);
}

UNumberFormatter* APS5_VABI unumf_openForSkeletonAndLocale_nid_postfix(const char16_t* skeleton,
                                                                       std::int32_t skeletonLen, const char* locale,
                                                                       std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    if ((!skeleton && skeletonLen != 0) || skeletonLen < -1) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    const std::u16string hostSkeleton =
        skeleton ? ReadGuestU16(skeleton, skeletonLen, __func__) : std::u16string{};
    std::string hostLocale;
    const char* localePtr = locale ? (hostLocale = ReadGuestCString(locale, __func__), hostLocale.c_str()) : uloc_getDefault();
    auto local = incoming;
    void* formatter = Icu::NumberFormatterBackend::OpenForSkeletonAndLocale(
        skeleton ? hostSkeleton.c_str() : nullptr, static_cast<std::int32_t>(hostSkeleton.size()), localePtr, &local);
    WriteStatus(status, incoming, local, __func__);
    if (formatter) RegisterFormatter(formatter);
    return static_cast<UNumberFormatter*>(formatter);
}

UFormattedNumber* APS5_VABI unumf_openResult_nid_postfix(std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return nullptr;
    auto local = incoming;
    void* result = Icu::NumberFormatterBackend::OpenResult(&local);
    WriteStatus(status, incoming, local, __func__);
    if (local <= 0 && result) RegisterResult(result);
    return static_cast<UFormattedNumber*>(result);
}

void APS5_VABI unumf_resultGetAllFieldPositions_nid_postfix(const UFormattedNumber* result,
                                                              UFieldPositionIterator* fpositer,
                                                              std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return;
    if (!result) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return;
    }
    if (!IsRegisteredResult(result)) NotImplemented_nid_no_patch(__func__);
    if (!fpositer) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return;
    }
    if (!Icu::IsRegisteredFieldPositionIterator(fpositer)) NotImplemented_nid_no_patch(__func__);
    auto local = incoming;
    Icu::NumberFormatterBackend::ResultGetAllFieldPositions(result, fpositer, &local);
    WriteStatus(status, incoming, local, __func__);
}

std::int32_t APS5_VABI unumf_resultToString_nid_postfix(const UFormattedNumber* result, char16_t* buffer,
                                                        std::int32_t bufferCapacity, std::int32_t* status) {
    const auto incoming = ReadStatus(status, __func__);
    if (incoming > 0) return 0;
    if (!result) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return 0;
    }
    if (!IsRegisteredResult(result)) NotImplemented_nid_no_patch(__func__);
    if ((buffer == nullptr ? bufferCapacity != 0 : bufferCapacity < 0)) {
        WriteStatus(status, incoming, Icu::IllegalArgument, __func__);
        return 0;
    }
    std::vector<char16_t> hostBuffer(bufferCapacity > 0 ? static_cast<std::size_t>(bufferCapacity) : 0u);
    auto local = incoming;
    const std::int32_t length = Icu::NumberFormatterBackend::ResultToString(
        result, bufferCapacity > 0 ? hostBuffer.data() : nullptr, bufferCapacity, &local);
    WriteGuestU16Result(buffer, bufferCapacity, hostBuffer, length, local, __func__);
    WriteStatus(status, incoming, local, __func__);
    return length;
}

}
