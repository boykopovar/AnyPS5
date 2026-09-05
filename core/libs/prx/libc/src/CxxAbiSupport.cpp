#include "exception/Runtime.hpp"
#include <thread>
#include <cstddef>
#include <cxxabi.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <typeinfo>
#include <new>
#include <ios>
#include <locale>
#include <regex>
#include <functional>
#include <unwind.h>

namespace LibcGuard {
thread_local std::uint64_t* active[64] {};
thread_local std::size_t depth {};
}

extern "C" {

int __cxa_guard_acquire_nid_postfix(std::uint64_t* guardObject) {
    auto* bytes = reinterpret_cast<unsigned char*>(guardObject);
    std::atomic_ref<unsigned char> initialized(bytes[0]);
    std::atomic_ref<unsigned char> lock(bytes[1]);
    if (initialized.load(std::memory_order_acquire)) return 0;
    for (std::size_t i = 0; i < LibcGuard::depth; ++i)
        if (LibcGuard::active[i] == guardObject) LibcException::Terminate();
    for (;;) {
        unsigned char expected = 0;
        if (lock.compare_exchange_weak(expected, 1, std::memory_order_acquire)) break;
        if (initialized.load(std::memory_order_acquire)) return 0;
        std::this_thread::yield();
    }
    if (initialized.load(std::memory_order_acquire)) { lock.store(0, std::memory_order_release); return 0; }
    if (LibcGuard::depth == 64) LibcException::Terminate();
    LibcGuard::active[LibcGuard::depth++] = guardObject;
    return 1;
}

void __cxa_guard_release_nid_postfix(std::uint64_t* guardObject) {
    if (!LibcGuard::depth || LibcGuard::active[LibcGuard::depth - 1] != guardObject) LibcException::Terminate();
    --LibcGuard::depth;
    auto* bytes = reinterpret_cast<unsigned char*>(guardObject);
    std::atomic_ref<unsigned char>(bytes[0]).store(1, std::memory_order_release);
    std::atomic_ref<unsigned char>(bytes[1]).store(0, std::memory_order_release);
}
void __cxa_guard_abort_nid_postfix(std::uint64_t* guardObject) {
    if (!LibcGuard::depth || LibcGuard::active[LibcGuard::depth - 1] != guardObject) LibcException::Terminate();
    --LibcGuard::depth;
    auto* bytes = reinterpret_cast<unsigned char*>(guardObject);
    std::atomic_ref<unsigned char>(bytes[1]).store(0, std::memory_order_release);
}

void* __cxa_demangle_nid_postfix(const char* mangled, char* buf, std::size_t* len, int* status) {
    (void)mangled; (void)buf; (void)len;
    if (status) *status = -2;
    return nullptr;
}

int __cxa_thread_atexit_impl_nid_postfix(void (*func)(void*), void* arg, void* dso) {
    (void)func; (void)arg; (void)dso;
    return 0;
}

void _ZNSt8ios_baseD2Ev_nid_postfix(std::ios_base* self) { self->~ios_base(); }
const std::error_category* _ZSt17iostream_categoryv_nid_postfix() { return &std::iostream_category(); }

}
