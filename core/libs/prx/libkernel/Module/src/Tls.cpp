#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
constexpr std::size_t ThreadLocalStoragePointerOffset = 0x58;
#endif

template <typename T>
bool CopyGuestInPreservingErrno(T& destination, const void* source) {
    if (source == nullptr) return false;
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    errno = savedErrno;
    return ok;
}

}

#if defined(__linux__)
extern "C" void* __tls_get_addr(void* index);
#endif

extern "C" {

void* APS5_VABI __tls_get_addr_nid_postfix(const KernelTlsIndex* index) {
    KernelTlsIndex guestIndex{};
    if (!CopyGuestInPreservingErrno(guestIndex, index) || guestIndex.module == 0) {
        throw std::invalid_argument("__tls_get_addr: null TLS index or module");
    }
#if defined(_WIN32)
    DWORD slot = 0;
    if (!CopyGuestInPreservingErrno(slot, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(guestIndex.module)))) {
        throw std::invalid_argument("__tls_get_addr: unreadable TLS module index");
    }
    auto* blocks = *reinterpret_cast<char***>(reinterpret_cast<char*>(NtCurrentTeb()) + ThreadLocalStoragePointerOffset);
    if (blocks == nullptr || blocks[slot] == nullptr) throw std::logic_error("__tls_get_addr: thread has no TLS block for the module");
    return blocks[slot] + guestIndex.offset;
#elif defined(__linux__)
    return __tls_get_addr(&guestIndex);
#else
    NotImplemented_nid_no_patch("__tls_get_addr on this host");
    return nullptr;
#endif
}

}
