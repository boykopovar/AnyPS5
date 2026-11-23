#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/AtomicWaitForwards.hpp"

#include <cstddef>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

struct Call {
    const void* address = nullptr;
    std::uint64_t expected = 0;
    bool timed = false;
    KernelUseconds timeout = 0;
    const char* name = nullptr;
    std::int32_t count = 0;
    int calls = 0;
};

Call last;

int APS5_VABI Wait8(std::uint8_t* address, std::uint8_t expected, KernelUseconds* timeout, const char* name) {
    last = {address, expected, timeout != nullptr, timeout ? *timeout : 0, name, 0, last.calls + 1};
    return static_cast<int>(0x8002003c);
}

int APS5_VABI Wait16(std::uint16_t* address, std::uint16_t expected, KernelUseconds* timeout, const char* name) {
    last = {address, expected, timeout != nullptr, timeout ? *timeout : 0, name, 0, last.calls + 1};
    return static_cast<int>(0x80020016);
}

int APS5_VABI Wait32(std::uint32_t* address, std::uint32_t expected, KernelUseconds* timeout, const char* name) {
    last = {address, expected, timeout != nullptr, timeout ? *timeout : 0, name, 0, last.calls + 1};
    return static_cast<int>(0x8002003c);
}

int APS5_VABI Wait64(std::uint64_t* address, std::uint64_t expected, KernelUseconds* timeout, const char* name) {
    last = {address, expected, timeout != nullptr, timeout ? *timeout : 0, name, 0, last.calls + 1};
    return static_cast<int>(0x80020016);
}

int APS5_VABI Wake(void* address, std::int32_t count) {
    last = {address, 0, false, 0, nullptr, count, last.calls + 1};
    return 0;
}

void Require(bool condition, const char* label) {
    if (condition) return;
    std::fprintf(stderr, "std atomic wait: %s\n", label);
    std::abort();
}

template <typename T, typename Fn>
void CheckWait(Fn wait, T value, std::uint64_t milliseconds, bool timed, KernelUseconds timeout, const char* label) {
    T storage = value;
    const int calls = last.calls;
    Require(wait(&storage, value, milliseconds) == 1, label);
    Require(last.calls == calls + 1 && last.address == &storage && last.expected == value, label);
    Require(last.timed == timed && last.timeout == timeout, label);
    Require(last.name != nullptr && std::strcmp(last.name, "SceLibcV2AtomicWait") == 0, label);
}

void CheckReadOnlyLibcxxAtomic() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    void* page = VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr, "allocate read-write page");
#else
    const long rawPageSize = sysconf(_SC_PAGESIZE);
    Require(rawPageSize > 0, "query page size");
    const std::size_t pageSize = static_cast<std::size_t>(rawPageSize);
    void* page = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED, "allocate read-write page");
#endif

    const auto* roPtr = new (page) std::int64_t(-42);

#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, pageSize, PAGE_READONLY, &previous) != 0, "protect page read-only");
#else
    Require(mprotect(page, pageSize, PROT_READ) == 0, "protect page read-only");
#endif

    errno = ESRCH;
    Require(_ZNSt3__123__libcpp_atomic_monitorEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(roPtr) == -42,
            "libc++ monitor reads read-only page");
    Require(errno == ESRCH, "libc++ monitor on read-only page preserves errno");

    last = {};
    _ZNSt3__120__libcpp_atomic_waitEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEEl_nid_postfix(roPtr, -42);
    Require(last.address == roPtr && last.expected == static_cast<std::uint64_t>(-42) && !last.timed,
            "libc++ wait accepts read-only page");
    Require(errno == ESRCH, "libc++ wait on read-only page preserves errno");

    last = {};
    _ZNSt3__123__cxx_atomic_notify_oneEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(roPtr);
    Require(last.address == roPtr && last.count == 1,
            "libc++ notify one accepts read-only page");
    Require(errno == ESRCH, "libc++ notify one on read-only page preserves errno");

#ifdef _WIN32
    Require(VirtualProtect(page, pageSize, PAGE_NOACCESS, &previous) != 0, "protect page no-access");
#else
    Require(mprotect(page, pageSize, PROT_NONE) == 0, "protect page no-access");
#endif

    errno = ESRCH;
    try {
        _ZNSt3__123__libcpp_atomic_monitorEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(roPtr);
        std::abort();
    } catch (const std::invalid_argument&) {}
    Require(errno == ESRCH, "libc++ monitor on no-access page preserves errno");

    errno = ESRCH;
    try {
        _ZNSt3__120__libcpp_atomic_waitEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEEl_nid_postfix(roPtr, -42);
        std::abort();
    } catch (const std::invalid_argument&) {}
    Require(errno == ESRCH, "libc++ wait on no-access page preserves errno");

    errno = ESRCH;
    try {
        _ZNSt3__123__cxx_atomic_notify_oneEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(roPtr);
        std::abort();
    } catch (const std::invalid_argument&) {}
    Require(errno == ESRCH, "libc++ notify one on no-access page preserves errno");

#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0, "release page");
#else
    Require(munmap(page, pageSize) == 0, "release page");
#endif
}

}

int main() {
    static const AtomicWaitForwards_nid_no_patch forwards{Wait8, Wait16, Wait32, Wait64, Wake};
    RegisterAtomicWaitForwards_nid_no_patch(&forwards);

    CheckWait<std::uint8_t>(__std_atomic_wait_direct_8_nid_postfix, 0xab, 0xffffffff, false, 0, "8-bit infinite wait passes no timeout");
    CheckWait<std::uint8_t>(__std_atomic_wait_direct_8_nid_postfix, 0xab, 0, true, 0, "8-bit zero milliseconds");
    CheckWait<std::uint8_t>(__std_atomic_wait_direct_8_nid_postfix, 0xab, 5, true, 5000, "8-bit milliseconds become microseconds");
    CheckWait<std::uint8_t>(__std_atomic_wait_direct_8_nid_postfix, 0xab, 0x418937, true, 4294967000u, "8-bit largest exact conversion");
    CheckWait<std::uint8_t>(__std_atomic_wait_direct_8_nid_postfix, 0xab, 0x418938, true, 0xffffffffu, "8-bit first clamped timeout");
    CheckWait<std::uint8_t>(__std_atomic_wait_direct_8_nid_postfix, 0xab, 0x100000000ull, true, 0xffffffffu, "8-bit timeouts wider than 32 bits are clamped");

    CheckWait<std::uint16_t>(__std_atomic_wait_direct_16_nid_postfix, 0xcafe, 0xffffffff, false, 0, "16-bit infinite wait passes no timeout");
    CheckWait<std::uint16_t>(__std_atomic_wait_direct_16_nid_postfix, 0xcafe, 0, true, 0, "16-bit zero milliseconds");
    CheckWait<std::uint16_t>(__std_atomic_wait_direct_16_nid_postfix, 0xcafe, 5, true, 5000, "16-bit milliseconds become microseconds");
    CheckWait<std::uint16_t>(__std_atomic_wait_direct_16_nid_postfix, 0xcafe, 0x418937, true, 4294967000u, "16-bit largest exact conversion");
    CheckWait<std::uint16_t>(__std_atomic_wait_direct_16_nid_postfix, 0xcafe, 0x418938, true, 0xffffffffu, "16-bit first clamped timeout");
    CheckWait<std::uint16_t>(__std_atomic_wait_direct_16_nid_postfix, 0xcafe, 0x100000000ull, true, 0xffffffffu, "16-bit timeouts wider than 32 bits are clamped");

    CheckWait<std::uint32_t>(__std_atomic_wait_direct_32_nid_postfix, 7, 0xffffffff, false, 0, "infinite wait passes no timeout");
    CheckWait<std::uint32_t>(__std_atomic_wait_direct_32_nid_postfix, 7, 0, true, 0, "zero milliseconds");
    CheckWait<std::uint32_t>(__std_atomic_wait_direct_32_nid_postfix, 7, 5, true, 5000, "milliseconds become microseconds");
    CheckWait<std::uint32_t>(__std_atomic_wait_direct_32_nid_postfix, 7, 0x418937, true, 4294967000u, "largest exact conversion");
    CheckWait<std::uint32_t>(__std_atomic_wait_direct_32_nid_postfix, 7, 0x418938, true, 0xffffffffu, "first clamped timeout");
    CheckWait<std::uint32_t>(__std_atomic_wait_direct_32_nid_postfix, 7, 0x100000000ull, true, 0xffffffffu, "timeouts wider than 32 bits are clamped");

    std::uint64_t wide = 0x1122334455667788ull;
    Require(__std_atomic_wait_direct_64_nid_postfix(&wide, 0x1122334455667788ull, 12) == 1, "64-bit wait returns 1");
    Require(last.address == &wide && last.expected == 0x1122334455667788ull && last.timed && last.timeout == 12000, "64-bit wait arguments");

    __std_atomic_notify_one_direct_nid_postfix(&wide);
    Require(last.address == &wide && last.count == 1, "notify one wakes one waiter");
    __std_atomic_notify_all_direct_nid_postfix(&wide);
    Require(last.address == &wide && last.count == 0x7fffffff, "notify all wakes every waiter");

    alignas(8) std::int64_t libcxx = -7;
    errno = ESRCH;
    Require(_ZNSt3__123__libcpp_atomic_monitorEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(&libcxx) == -7,
            "libc++ monitor reads the signed 64-bit value");
    Require(errno == ESRCH, "libc++ monitor preserves errno");
    _ZNSt3__120__libcpp_atomic_waitEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEEl_nid_postfix(&libcxx, -7);
    Require(last.address == &libcxx && last.expected == static_cast<std::uint64_t>(-7) && !last.timed,
            "libc++ wait forwards the signed 64-bit value without a timeout");
    _ZNSt3__123__cxx_atomic_notify_oneEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(&libcxx);
    Require(last.address == &libcxx && last.count == 1, "libc++ notify one wakes one waiter");
    try { _ZNSt3__123__libcpp_atomic_monitorEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(nullptr); std::abort(); }
    catch (const std::invalid_argument&) {}
    try { _ZNSt3__120__libcpp_atomic_waitEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEEl_nid_postfix(reinterpret_cast<const std::int64_t*>(1), -7); std::abort(); }
    catch (const std::invalid_argument&) {}
    try { _ZNSt3__123__cxx_atomic_notify_oneEPVKNS_17__cxx_atomic_implIlNS_22__cxx_atomic_base_implIlEEEE_nid_postfix(nullptr); std::abort(); }
    catch (const std::invalid_argument&) {}

    CheckReadOnlyLibcxxAtomic();

    RegisterAtomicWaitForwards_nid_no_patch(nullptr);
    return 0;
}
