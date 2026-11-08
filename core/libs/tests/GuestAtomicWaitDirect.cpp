#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static void Require(bool value) { if (!value) std::abort(); }

namespace {

struct Record {
    int wakes = 0;
    void* wakeAddress = nullptr;
    std::int32_t wakeCount = 0;
    int waits32 = 0;
    std::uint32_t expected32 = 0;
    unsigned int* timeout32 = nullptr;
    unsigned int timeoutValue32 = 0;
    int waits64 = 0;
    std::uint64_t expected64 = 0;
    unsigned int* timeout64 = nullptr;
    unsigned int timeoutValue64 = 0;
};

Record record;

int APS5_VABI RecordWake(void* address, std::int32_t count) {
    record.wakes += 1;
    record.wakeAddress = address;
    record.wakeCount = count;
    return 0;
}

int APS5_VABI RecordWait32(std::uint32_t* address, std::uint32_t expected, unsigned int* timeout, const char*) {
    record.waits32 += 1;
    record.expected32 = expected;
    record.timeout32 = timeout;
    record.timeoutValue32 = timeout == nullptr ? 0u : *timeout;
    return *address == expected ? 0 : 0x8002000d;
}

int APS5_VABI RecordWait64(std::uint64_t* address, std::uint64_t expected, unsigned int* timeout, const char*) {
    record.waits64 += 1;
    record.expected64 = expected;
    record.timeout64 = timeout;
    record.timeoutValue64 = timeout == nullptr ? 0u : *timeout;
    return *address == expected ? 0 : 0x8002000d;
}

void Reset() {
    record = Record{};
    RegisterSyncOnAddress_nid_no_patch(&RecordWake, &RecordWait32, &RecordWait64);
}

void Forwarding() {
    Reset();
    std::uint32_t word = 7;
    std::uint64_t wide = 9;

    __std_atomic_notify_one_direct_nid_postfix(&word);
    Require(record.wakes == 1 && record.wakeAddress == &word && record.wakeCount == 1);

    __std_atomic_notify_all_direct_nid_postfix(&word);
    Require(record.wakes == 2 && record.wakeAddress == &word && record.wakeCount == 0x7FFFFFFF);

    Require(__std_atomic_wait_direct_32_nid_postfix(&word, 7, 5) == true);
    Require(record.waits32 == 1 && record.expected32 == 7);
    Require(record.timeout32 != nullptr && record.timeoutValue32 == 5000);

    Require(__std_atomic_wait_direct_32_nid_postfix(&word, 7, 0xFFFFFFFFu) == true);
    Require(record.waits32 == 2 && record.timeout32 == nullptr);

    Require(__std_atomic_wait_direct_32_nid_postfix(&word, 7, 0x418938u) == true);
    Require(record.waits32 == 3 && record.timeout32 != nullptr && record.timeoutValue32 == 0xFFFFFFFFu);

    Require(__std_atomic_wait_direct_64_nid_postfix(&wide, 9, 1) == true);
    Require(record.waits64 == 1 && record.expected64 == 9);
    Require(record.timeout64 != nullptr && record.timeoutValue64 == 1000);

    Require(__std_atomic_wait_direct_64_nid_postfix(&wide, 9, 0xFFFFFFFFu) == true);
    Require(record.waits64 == 2 && record.timeout64 == nullptr);
}

void Integration() {
    RegisterSyncOnAddress_nid_no_patch(&sceKernelSyncOnAddressWake, &sceKernelSyncOnAddressWait32, &sceKernelSyncOnAddressWait64);

    std::uint32_t word = 5;
    const auto before = std::chrono::steady_clock::now();
    Require(__std_atomic_wait_direct_32_nid_postfix(&word, 4, 5000) == true);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count();
    Require(elapsed < 1000);

    Require(__std_atomic_wait_direct_32_nid_postfix(&word, 5, 0) == true);

    std::uint64_t wide = 11;
    Require(__std_atomic_wait_direct_64_nid_postfix(&wide, 10, 5000) == true);

    std::atomic<bool> woken{false};
    word = 21;
    std::thread waiter([&] {
        __std_atomic_wait_direct_32_nid_postfix(&word, 21, 4000);
        woken.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    word = 22;
    __std_atomic_notify_one_direct_nid_postfix(&word);
    waiter.join();
    Require(woken.load());
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    if (std::strcmp(argv[1], "forwarding") == 0) Forwarding();
    else if (std::strcmp(argv[1], "integration") == 0) Integration();
    else Require(false);
    std::puts("atomic wait forwarding and kernel wake passed");
    return 0;
}
