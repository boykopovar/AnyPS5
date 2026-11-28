#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void RequireReadable(const void* source, std::size_t bytes, const char* message) {
    if (!bytes) return;
    ErrnoGuard guard;
    if (!source) throw std::invalid_argument(message);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, bytes);
    if (!GuestReadable_nid_no_patch(source, bytes)) throw std::invalid_argument(message);
}

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!bytes) return;
    ErrnoGuard guard;
    if (!source) throw std::invalid_argument(message);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, bytes);
    if (!GuestReadable_nid_no_patch(source, bytes) || !GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

class GuestWriteScope {
public:
    GuestWriteScope(void* pointer, std::size_t bytes, const char* message)
        : saved(), guard(pointer ? pointer : nullptr, pointer ? bytes : 0) {
        if (!bytes) return;
        if (!pointer || !guard.Open() || !GuestWritable_nid_no_patch(pointer, bytes))
            throw std::invalid_argument(message);
    }
    GuestWriteScope(const GuestWriteScope&) = delete;
    GuestWriteScope& operator=(const GuestWriteScope&) = delete;

private:
    ErrnoGuard saved;
    GuestArena::HostWrite guard;
};

std::size_t CheckedRangeLength(const void* first, const void* last, const char* message) {
    const auto beginAddr = reinterpret_cast<std::uintptr_t>(first);
    const auto endAddr = reinterpret_cast<std::uintptr_t>(last);
    if (endAddr < beginAddr) throw std::invalid_argument(message);
    const std::size_t length = endAddr - beginAddr;
    if (length != 0 && !first) throw std::invalid_argument(message);
    return length;
}

}

extern "C" {

GuestCLocale::Ctypevec APS5_VABI _Getctype_nid_postfix() {
    return {_Getpctype_nid_postfix(), 0};
}

GuestCLocale::Collvec APS5_VABI _Getcoll_nid_postfix() {
    return {};
}

GuestCLocale::Cvtvec APS5_VABI _Getcvt_nid_postfix() {
    GuestCLocale::Cvtvec vec{};
    vec.page = 1;
    vec.maxBytes = 1;
    return vec;
}

int APS5_VABI _Getdateorder_nid_postfix() {
    return GuestCLocale::DateOrderNone;
}

int APS5_VABI _Strcoll_nid_postfix(const char* first1, const char* last1, const char* first2, const char* last2, const void*) {
    const std::size_t length1 = CheckedRangeLength(first1, last1, "_Strcoll: invalid first range");
    const std::size_t length2 = CheckedRangeLength(first2, last2, "_Strcoll: invalid second range");
    RequireReadable(first1, length1, "_Strcoll: unreadable first range");
    RequireReadable(first2, length2, "_Strcoll: unreadable second range");
    const auto addr1 = reinterpret_cast<std::uintptr_t>(first1);
    const auto addr2 = reinterpret_cast<std::uintptr_t>(first2);
    const std::size_t common = std::min(length1, length2);
    unsigned char chunk1[4096];
    unsigned char chunk2[4096];
    for (std::size_t offset = 0; offset < common;) {
        const std::size_t step = std::min<std::size_t>(sizeof(chunk1), common - offset);
        CopyIn(chunk1, reinterpret_cast<const void*>(addr1 + offset), step, "_Strcoll: unreadable first range");
        CopyIn(chunk2, reinterpret_cast<const void*>(addr2 + offset), step, "_Strcoll: unreadable second range");
        for (std::size_t i = 0; i < step; ++i) {
            const int difference = static_cast<int>(chunk1[i]) - static_cast<int>(chunk2[i]);
            if (difference != 0) return difference;
        }
        offset += step;
    }
    if (length1 == length2) return 0;
    return length1 > length2 ? 1 : -1;
}

std::size_t APS5_VABI _Strxfrm_nid_postfix(char* dest, char* destEnd, const char* src, const char* srcEnd, const void*) {
    const std::size_t capacity = CheckedRangeLength(dest, destEnd, "_Strxfrm: invalid destination range");
    const std::size_t length = CheckedRangeLength(src, srcEnd, "_Strxfrm: invalid source range");
    RequireReadable(src, length, "_Strxfrm: unreadable source range");
    if (length != 0 && length <= capacity) {
        GuestWriteScope destWrite(dest, length, "_Strxfrm: unwritable destination range");
        const auto srcAddr = reinterpret_cast<std::uintptr_t>(src);
        const auto destAddr = reinterpret_cast<std::uintptr_t>(dest);
        const bool overlaps = srcAddr < destAddr + length && destAddr < srcAddr + length;
        if (overlaps) {
            std::vector<unsigned char> staging(length);
            CopyIn(staging.data(), src, length, "_Strxfrm: unreadable source range");
            if (!GuestCopyOut_nid_no_patch(dest, staging.data(), length))
                throw std::invalid_argument("_Strxfrm: unwritable destination range");
        } else {
            unsigned char chunk[4096];
            for (std::size_t offset = 0; offset < length;) {
                const std::size_t step = std::min<std::size_t>(sizeof(chunk), length - offset);
                CopyIn(chunk, reinterpret_cast<const void*>(srcAddr + offset), step, "_Strxfrm: unreadable source range");
                if (!GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(destAddr + offset), chunk, step))
                    throw std::invalid_argument("_Strxfrm: unwritable destination range");
                offset += step;
            }
        }
    }
    return length;
}

}
