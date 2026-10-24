#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

struct TlsIndex {
    std::uint64_t module;
    std::uint64_t offset;
};

#ifdef _WIN32
constexpr std::size_t ThreadLocalStoragePointerOffset = 0x58;
#endif

}

#if defined(__linux__)
extern "C" void* __tls_get_addr(void* index);
#endif

extern "C" {

void* APS5_VABI __tls_get_addr_nid_postfix(const TlsIndex* index) {
    if (index == nullptr || index->module == 0) throw std::invalid_argument("__tls_get_addr: null TLS index or module");
#if defined(_WIN32)
    const auto slot = *reinterpret_cast<const DWORD*>(static_cast<std::uintptr_t>(index->module));
    auto* blocks = *reinterpret_cast<char***>(reinterpret_cast<char*>(NtCurrentTeb()) + ThreadLocalStoragePointerOffset);
    if (blocks == nullptr || blocks[slot] == nullptr) throw std::logic_error("__tls_get_addr: thread has no TLS block for the module");
    return blocks[slot] + index->offset;
#elif defined(__linux__)
    TlsIndex host = *index;
    return __tls_get_addr(&host);
#else
    NotImplemented_nid_no_patch("__tls_get_addr on this host");
    return nullptr;
#endif
}

}
