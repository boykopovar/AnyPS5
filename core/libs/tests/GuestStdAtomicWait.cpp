#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/AtomicWaitForwards.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

void CheckWait32(std::uint64_t milliseconds, bool timed, KernelUseconds timeout, const char* label) {
    std::uint32_t word = 7;
    const int calls = last.calls;
    Require(__std_atomic_wait_direct_32_nid_postfix(&word, 7, milliseconds) == 1, label);
    Require(last.calls == calls + 1 && last.address == &word && last.expected == 7, label);
    Require(last.timed == timed && last.timeout == timeout, label);
    Require(last.name != nullptr && std::strcmp(last.name, "SceLibcV2AtomicWait") == 0, label);
}

}

int main() {
    static const AtomicWaitForwards_nid_no_patch forwards{Wait32, Wait64, Wake};
    RegisterAtomicWaitForwards_nid_no_patch(&forwards);

    CheckWait32(0xffffffff, false, 0, "infinite wait passes no timeout");
    CheckWait32(0, true, 0, "zero milliseconds");
    CheckWait32(5, true, 5000, "milliseconds become microseconds");
    CheckWait32(0x418937, true, 4294967000u, "largest exact conversion");
    CheckWait32(0x418938, true, 0xffffffffu, "first clamped timeout");
    CheckWait32(0x100000000ull, true, 0xffffffffu, "timeouts wider than 32 bits are clamped");

    std::uint64_t wide = 0x1122334455667788ull;
    Require(__std_atomic_wait_direct_64_nid_postfix(&wide, 0x1122334455667788ull, 12) == 1, "64-bit wait returns 1");
    Require(last.address == &wide && last.expected == 0x1122334455667788ull && last.timed && last.timeout == 12000, "64-bit wait arguments");

    __std_atomic_notify_one_direct_nid_postfix(&wide);
    Require(last.address == &wide && last.count == 1, "notify one wakes one waiter");
    __std_atomic_notify_all_direct_nid_postfix(&wide);
    Require(last.address == &wide && last.count == 0x7fffffff, "notify all wakes every waiter");

    RegisterAtomicWaitForwards_nid_no_patch(nullptr);
    return 0;
}
