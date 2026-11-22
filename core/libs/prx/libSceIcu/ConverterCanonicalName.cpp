#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

namespace {

constexpr auto HostGetCanonicalName = &ucnv_getCanonicalName;

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = saved;
    return copied;
}

std::string CopyText(const char* source, const char* function) {
    std::string result;
    const auto base = reinterpret_cast<std::uintptr_t>(source);
    std::size_t offset = 0;
    for (;;) {
        if (offset > std::numeric_limits<std::uintptr_t>::max() - base) NotImplemented_nid_no_patch(function);
        const auto current = base + offset;
        const std::size_t pageRemaining = 4096 - current % 4096;
        const std::size_t count = pageRemaining < 256 ? pageRemaining : 256;
        char buffer[256];
        if (!CopyIn(buffer, reinterpret_cast<const void*>(current), count)) NotImplemented_nid_no_patch(function);
        const auto* end = static_cast<const char*>(std::memchr(buffer, 0, count));
        result.append(buffer, end ? static_cast<std::size_t>(end - buffer) : count);
        if (end) return result;
        offset += count;
    }
}

}

#undef ucnv_getCanonicalName

extern "C" const char* APS5_VABI ucnv_getCanonicalName(const char* alias, const char* standard, std::int32_t* status) {
    if (!status) NotImplemented_nid_no_patch(__func__);
    std::int32_t guestStatus = 0;
    if (!CopyIn(&guestStatus, status, sizeof(guestStatus))) NotImplemented_nid_no_patch(__func__);
    UErrorCode hostStatus = static_cast<UErrorCode>(guestStatus);
    const auto finish = [&](const char* result) {
        const auto finalStatus = static_cast<std::int32_t>(hostStatus);
        if (finalStatus != guestStatus && !CopyOut(status, &finalStatus, sizeof(finalStatus))) NotImplemented_nid_no_patch(__func__);
        return result;
    };
    if (U_FAILURE(hostStatus)) return finish(HostGetCanonicalName(nullptr, nullptr, &hostStatus));
    UErrorCode probeStatus = hostStatus;
    HostGetCanonicalName("", "", &probeStatus);
    if (U_FAILURE(probeStatus)) {
        hostStatus = probeStatus;
        return finish(nullptr);
    }
    if (!alias) return finish(HostGetCanonicalName(nullptr, nullptr, &hostStatus));
    const std::string aliasCopy = CopyText(alias, __func__);
    if (aliasCopy.empty()) return finish(HostGetCanonicalName(aliasCopy.c_str(), nullptr, &hostStatus));
    if (!standard) NotImplemented_nid_no_patch(__func__);
    const std::string standardCopy = CopyText(standard, __func__);
    return finish(HostGetCanonicalName(aliasCopy.c_str(), standardCopy.c_str(), &hostStatus));
}
