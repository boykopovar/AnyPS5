#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr int BignumLibrary = 3;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int BignumTooLong = 114;
constexpr int FlagMalloced = 0x01;
constexpr int FlagConstantTime = 0x04;
constexpr int MaxWords = INT_MAX / (4 * 64);

bool CopyGuestInPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

bool GuestReadable(const void* source, std::size_t bytes) {
    std::array<unsigned char, 4096> chunk;
    for (std::size_t done = 0; done < bytes;) {
        const std::size_t count = std::min(chunk.size(), bytes - done);
        if (!CopyGuestInPreservingErrno(chunk.data(), static_cast<const unsigned char*>(source) + done, count))
            return false;
        done += count;
    }
    return true;
}

Bignum* Fail(Bignum* copy, int reason) {
    LibreSslPutError_nid_no_patch(BignumLibrary, AnyFunction, reason);
    if (copy) BN_free_nid_postfix(copy);
    return nullptr;
}

}

extern "C" Bignum* APS5_VABI BN_dup_nid_postfix(const Bignum* source) {
    if (!source) return nullptr;
    Bignum value{};
    if (!CopyGuestInPreservingErrno(&value, source, sizeof(value))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (value.top > 0 && !value.d) throw std::invalid_argument("BN_dup: BIGNUM with words but no storage");
    auto* copy = static_cast<Bignum*>(calloc_nid_postfix(1, sizeof(Bignum)));
    if (!copy) return Fail(nullptr, MallocFailure);
    copy->flags = FlagMalloced;
    if (value.top < 0) {
        BN_free_nid_postfix(copy);
        return nullptr;
    }
    if (value.top > MaxWords) return Fail(copy, BignumTooLong);
    if (value.top > 0) {
        const std::size_t bytes = static_cast<std::size_t>(value.top) * sizeof(std::uint64_t);
        if (!GuestReadable(value.d, bytes)) {
            BN_free_nid_postfix(copy);
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        }
        auto* words = static_cast<std::uint64_t*>(calloc_nid_postfix(static_cast<std::size_t>(value.top), sizeof(std::uint64_t)));
        if (!words) return Fail(copy, MallocFailure);
        if (!CopyGuestInPreservingErrno(words, value.d, bytes)) {
            free_nid_postfix(words);
            BN_free_nid_postfix(copy);
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        }
        copy->d = words;
        copy->dmax = value.top;
    }
    copy->top = value.top;
    copy->neg = value.neg;
    copy->flags |= value.flags & FlagConstantTime;
    return copy;
}
