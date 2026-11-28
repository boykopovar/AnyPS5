#include "prx/libSceIcu/IdnaBackend.hpp"
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

std::mutex g_idnaMutex;
std::unordered_set<const void*> g_openIdna;

void RegisterIdna(const void* idna) {
    if (!idna) return;
    std::lock_guard lock(g_idnaMutex);
    g_openIdna.insert(idna);
}

bool IsRegisteredIdna(const void* idna) {
    if (!idna) return false;
    std::lock_guard lock(g_idnaMutex);
    return g_openIdna.contains(idna);
}

bool UnregisterIdna(const void* idna) {
    if (!idna) return false;
    std::lock_guard lock(g_idnaMutex);
    return g_openIdna.erase(idna) > 0;
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

template<typename TCall>
std::int32_t TransformName(const UIDNA* idna, const char16_t* name, std::int32_t length, char16_t* dest,
                           std::int32_t capacity, UIDNAInfo* pInfo, std::int32_t* pErrorCode,
                           const char* caller, TCall call) {
    const auto incoming = ReadStatus(pErrorCode, caller);
    if (incoming > 0) return 0;
    if (!pInfo) {
        WriteStatus(pErrorCode, incoming, Icu::IllegalArgument, caller);
        return 0;
    }
    std::int16_t guestInfoSize = 0;
    {
        const int savedErrno = errno;
        const bool okSize = GuestCopyIn_nid_no_patch(&guestInfoSize, pInfo, sizeof(guestInfoSize));
        errno = savedErrno;
        if (!okSize) NotImplemented_nid_no_patch(caller);
    }
    if (guestInfoSize < static_cast<std::int16_t>(sizeof(Icu::IdnaBackend::Info))) {
        WriteStatus(pErrorCode, incoming, Icu::IllegalArgument, caller);
        return 0;
    }
    Icu::IdnaBackend::Info hostInfo{};
    hostInfo.size = static_cast<std::int16_t>(sizeof(hostInfo));
    if ((name == nullptr) ? length != 0 : length < -1) {
        WriteStatus(pErrorCode, incoming, Icu::IllegalArgument, caller);
        return 0;
    }
    if ((dest == nullptr) ? capacity != 0 : capacity < 0) {
        WriteStatus(pErrorCode, incoming, Icu::IllegalArgument, caller);
        return 0;
    }
    if (dest == name && name != nullptr) {
        WriteStatus(pErrorCode, incoming, Icu::IllegalArgument, caller);
        return 0;
    }
    if (!idna || !IsRegisteredIdna(idna)) throw std::invalid_argument(std::string(caller) + ": invalid handle");
    std::u16string hostName;
    const char16_t* namePtr = nullptr;
    std::int32_t nameLen = length;
    if (name != nullptr) {
        hostName = ReadGuestU16String(name, length, caller);
        namePtr = hostName.c_str();
        nameLen = static_cast<std::int32_t>(hostName.size());
    }
    std::vector<char16_t> buffer(capacity > 0 ? static_cast<std::size_t>(capacity) : 0u);
    auto local = incoming;
    const std::int32_t outLen = call(
        idna, namePtr, nameLen, capacity > 0 ? buffer.data() : nullptr, capacity, &hostInfo, &local);
    hostInfo.size = guestInfoSize;
    {
        const auto totalBytes = static_cast<std::size_t>(guestInfoSize);
        std::vector<std::uint8_t> outInfo(totalBytes, 0);
        std::memcpy(outInfo.data(), &hostInfo, sizeof(hostInfo));
        const int savedErrno = errno;
        const bool ok = GuestCopyOut_nid_no_patch(pInfo, outInfo.data(), outInfo.size());
        errno = savedErrno;
        if (!ok) NotImplemented_nid_no_patch(caller);
    }
    if (capacity > 0 && outLen >= 0 && local != Icu::IllegalArgument) {
        const std::size_t requested = static_cast<std::size_t>(outLen) + (outLen < capacity ? 1u : 0u);
        const std::size_t count = (std::min)(buffer.size(), (std::min)(static_cast<std::size_t>(capacity), requested));
        if (count > 0) {
            const int savedErrno = errno;
            const bool ok = GuestCopyOut_nid_no_patch(dest, buffer.data(), count * sizeof(char16_t));
            errno = savedErrno;
            if (!ok) NotImplemented_nid_no_patch(caller);
        }
    }
    WriteStatus(pErrorCode, incoming, local, caller);
    return outLen;
}

}

extern "C" {

void APS5_VABI uidna_close_nid_postfix(UIDNA* idna) {
    if (!idna) return;
    if (!UnregisterIdna(idna)) NotImplemented_nid_no_patch(__func__);
    Icu::IdnaBackend::Close(idna);
}

std::int32_t APS5_VABI uidna_nameToASCII_nid_postfix(const UIDNA* idna, const char16_t* name, std::int32_t length,
                                                     char16_t* dest, std::int32_t capacity, UIDNAInfo* pInfo,
                                                     std::int32_t* pErrorCode) {
    return TransformName(idna, name, length, dest, capacity, pInfo, pErrorCode, __func__,
                         Icu::IdnaBackend::NameToASCII);
}

std::int32_t APS5_VABI uidna_nameToUnicode_nid_postfix(const UIDNA* idna, const char16_t* name, std::int32_t length,
                                                       char16_t* dest, std::int32_t capacity, UIDNAInfo* pInfo,
                                                       std::int32_t* pErrorCode) {
    return TransformName(idna, name, length, dest, capacity, pInfo, pErrorCode, __func__,
                         Icu::IdnaBackend::NameToUnicode);
}

UIDNA* APS5_VABI uidna_openUTS46_nid_postfix(std::uint32_t options, std::int32_t* pErrorCode) {
    const auto incoming = ReadStatus(pErrorCode, __func__);
    if (incoming > 0) return nullptr;
    auto local = incoming;
    void* idna = Icu::IdnaBackend::OpenUTS46(options, &local);
    WriteStatus(pErrorCode, incoming, local, __func__);
    if (local <= 0 && idna) RegisterIdna(idna);
    return static_cast<UIDNA*>(idna);
}

}
