#include "prx/libSceIcu/include/LegacyExports.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include "ulocimp.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>

namespace {

constexpr std::size_t MaximumStringLength = 0x10000;

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(caller);
}

std::string ReadString(const char* source, const char* caller) {
    std::string text;
    const int savedErrno = errno;
    const int error = CopyGuestString_nid_no_patch(source, MaximumStringLength, text);
    errno = savedErrno;
    if (error != 0) NotImplemented_nid_no_patch(caller);
    return text;
}

} // namespace

extern "C" {

const char16_t* APS5_VABI uloc_getTableStringWithFallback_nid_postfix(const char* path, const char* locale,
                                                                     const char* tableKey, const char* subTableKey,
                                                                     const char* itemKey, std::int32_t* pLength,
                                                                     std::int32_t* pErrorCode) {
    const char* caller = __func__;
    Icu::RequireStatus(pErrorCode);
    std::int32_t incoming = 0;
    CopyIn(&incoming, pErrorCode, sizeof(incoming), caller);
    if (incoming > 0) return nullptr;
    if (!tableKey || !itemKey || !pLength) NotImplemented_nid_no_patch(caller);

    const std::string hostPath = path ? ReadString(path, caller) : std::string();
    const std::string hostLocale = locale ? ReadString(locale, caller) : std::string();
    const std::string hostTableKey = ReadString(tableKey, caller);
    const std::string hostSubTableKey = subTableKey ? ReadString(subTableKey, caller) : std::string();
    const std::string hostItemKey = ReadString(itemKey, caller);

    UErrorCode errorCode = static_cast<UErrorCode>(incoming);
    std::int32_t length = 0;
    const char16_t* result = uloc_getTableStringWithFallback(
        path ? hostPath.c_str() : nullptr,
        locale ? hostLocale.c_str() : nullptr,
        hostTableKey.c_str(),
        subTableKey ? hostSubTableKey.c_str() : nullptr,
        hostItemKey.c_str(),
        &length,
        &errorCode);
    const auto outgoing = static_cast<std::int32_t>(errorCode);
    if (outgoing != incoming) CopyOut(pErrorCode, &outgoing, sizeof(outgoing), caller);
    if (result) CopyOut(pLength, &length, sizeof(length), caller);
    return result;
}

}
