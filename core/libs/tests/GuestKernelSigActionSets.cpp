#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static void APS5_VABI DummyHandler(int) {}

static void AuditSigactionUncatchableAndBounds() {
    GuestSigaction act{};
    act.handler = reinterpret_cast<std::uintptr_t>(DummyHandler);
    act.flags = 0;
    sigemptyset_nid_postfix(&act.mask);

    GuestSigaction old{};
    for (const int uncatchable : {9, 17}) {
        *__error_nid_postfix() = 0;
        Require(sigaction_nid_postfix(uncatchable, &act, nullptr) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(sigaction_nid_postfix(uncatchable, &act, &old) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(_sigaction_nid_postfix(uncatchable, &act, nullptr) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(_sigaction_nid_postfix(uncatchable, &act, &old) == -1 && *__error_nid_postfix() == 22);

        act.handler = 1;
        *__error_nid_postfix() = 0;
        Require(sigaction_nid_postfix(uncatchable, &act, nullptr) == -1 && *__error_nid_postfix() == 22);
        act.handler = reinterpret_cast<std::uintptr_t>(DummyHandler);

        *__error_nid_postfix() = 0;
        Require(sigaction_nid_postfix(uncatchable, nullptr, &old) == 0);
        *__error_nid_postfix() = 0;
        Require(_sigaction_nid_postfix(uncatchable, nullptr, &old) == 0);
    }

    for (const int invalid : {0, -1, -50, 129, 200, 1000, INT_MIN, INT_MAX}) {
        *__error_nid_postfix() = 0;
        Require(sigaction_nid_postfix(invalid, nullptr, &old) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(sigaction_nid_postfix(invalid, &act, nullptr) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(_sigaction_nid_postfix(invalid, nullptr, nullptr) == -1 && *__error_nid_postfix() == 22);
    }

    act.flags = 0x8000;
    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(30, &act, nullptr) == -1 && *__error_nid_postfix() == 22);
    act.flags = 0;

    GuestSigaction original{};
    Require(sigaction_nid_postfix(30, nullptr, &original) == 0);
    act.handler = reinterpret_cast<std::uintptr_t>(DummyHandler);
    act.flags = 0;
    Require(sigemptyset_nid_postfix(&act.mask) == 0);
    Require(sigaddset_nid_postfix(&act.mask, 15) == 0);
    GuestSigaction previous{};
    Require(sigaction_nid_postfix(30, &act, &previous) == 0);
    GuestSigaction current{};
    Require(sigaction_nid_postfix(30, nullptr, &current) == 0);
    Require(current.handler == reinterpret_cast<std::uintptr_t>(DummyHandler));
    Require(sigismember_nid_postfix(&current.mask, 15) == 1);
    Require(sigaction_nid_postfix(30, &original, nullptr) == 0);
}

static void AuditSigsetOperations() {
    GuestSignalSet set;
    std::memset(&set, 0xaa, sizeof(set));
    Require(sigemptyset_nid_postfix(&set) == 0);
    for (int i = 0; i < 4; ++i) {
        Require(set.bits[i] == 0);
    }
    for (int s = 1; s <= 128; ++s) {
        Require(sigismember_nid_postfix(&set, s) == 0);
    }

    Require(sigfillset_nid_postfix(&set) == 0);
    for (int i = 0; i < 4; ++i) {
        Require(set.bits[i] == 0xffffffffu);
    }
    for (int s = 1; s <= 128; ++s) {
        Require(sigismember_nid_postfix(&set, s) == 1);
    }

    Require(sigemptyset_nid_postfix(&set) == 0);
    Require(sigaddset_nid_postfix(&set, 1) == 0 && set.bits[0] == 1u);
    Require(sigaddset_nid_postfix(&set, 32) == 0 && set.bits[0] == 0x80000001u);
    Require(sigaddset_nid_postfix(&set, 33) == 0 && set.bits[1] == 1u);
    Require(sigaddset_nid_postfix(&set, 64) == 0 && set.bits[1] == 0x80000001u);
    Require(sigaddset_nid_postfix(&set, 65) == 0 && set.bits[2] == 1u);
    Require(sigaddset_nid_postfix(&set, 96) == 0 && set.bits[2] == 0x80000001u);
    Require(sigaddset_nid_postfix(&set, 97) == 0 && set.bits[3] == 1u);
    Require(sigaddset_nid_postfix(&set, 128) == 0 && set.bits[3] == 0x80000001u);

    Require(sigismember_nid_postfix(&set, 1) == 1);
    Require(sigismember_nid_postfix(&set, 2) == 0);
    Require(sigismember_nid_postfix(&set, 32) == 1);
    Require(sigismember_nid_postfix(&set, 33) == 1);
    Require(sigismember_nid_postfix(&set, 64) == 1);
    Require(sigismember_nid_postfix(&set, 65) == 1);
    Require(sigismember_nid_postfix(&set, 96) == 1);
    Require(sigismember_nid_postfix(&set, 97) == 1);
    Require(sigismember_nid_postfix(&set, 128) == 1);
    Require(sigismember_nid_postfix(&set, 127) == 0);

    Require(sigdelset_nid_postfix(&set, 1) == 0 && set.bits[0] == 0x80000000u);
    Require(sigdelset_nid_postfix(&set, 32) == 0 && set.bits[0] == 0);
    Require(sigdelset_nid_postfix(&set, 33) == 0 && set.bits[1] == 0x80000000u);
    Require(sigdelset_nid_postfix(&set, 64) == 0 && set.bits[1] == 0);
    Require(sigdelset_nid_postfix(&set, 65) == 0 && set.bits[2] == 0x80000000u);
    Require(sigdelset_nid_postfix(&set, 96) == 0 && set.bits[2] == 0);
    Require(sigdelset_nid_postfix(&set, 97) == 0 && set.bits[3] == 0x80000000u);
    Require(sigdelset_nid_postfix(&set, 128) == 0 && set.bits[3] == 0);

    for (int i = 0; i < 4; ++i) {
        Require(set.bits[i] == 0);
    }

    for (const int invalid : {0, -1, 129, 200, 1000, INT_MIN, INT_MAX}) {
        *__error_nid_postfix() = 0;
        Require(sigaddset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(sigdelset_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(sigismember_nid_postfix(&set, invalid) == -1 && *__error_nid_postfix() == 22);
    }
}

static void AuditSigprocmaskBoundariesAndUncatchable() {
    GuestSignalSet initialBlocked{};
    Require(sigprocmask_nid_postfix(1, nullptr, &initialBlocked) == 0);

    GuestSignalSet toBlock{};
    Require(sigemptyset_nid_postfix(&toBlock) == 0);
    Require(sigaddset_nid_postfix(&toBlock, 9) == 0);
    Require(sigaddset_nid_postfix(&toBlock, 17) == 0);
    Require(sigaddset_nid_postfix(&toBlock, 30) == 0);
    Require(sigprocmask_nid_postfix(1, &toBlock, nullptr) == 0);

    GuestSignalSet currentBlocked{};
    Require(sigprocmask_nid_postfix(1, nullptr, &currentBlocked) == 0);
    Require(sigismember_nid_postfix(&currentBlocked, 9) == 0);
    Require(sigismember_nid_postfix(&currentBlocked, 17) == 0);
    Require(sigismember_nid_postfix(&currentBlocked, 30) == 1);

    GuestSignalSet allSignals{};
    Require(sigfillset_nid_postfix(&allSignals) == 0);
    Require(sigprocmask_nid_postfix(3, &allSignals, nullptr) == 0);
    Require(sigprocmask_nid_postfix(1, nullptr, &currentBlocked) == 0);
    Require(sigismember_nid_postfix(&currentBlocked, 9) == 0);
    Require(sigismember_nid_postfix(&currentBlocked, 17) == 0);
    Require(sigismember_nid_postfix(&currentBlocked, 1) == 1);
    Require(sigismember_nid_postfix(&currentBlocked, 128) == 1);

    Require(sigprocmask_nid_postfix(2, &allSignals, nullptr) == 0);
    Require(sigprocmask_nid_postfix(1, nullptr, &currentBlocked) == 0);
    for (int s = 1; s <= 128; ++s) {
        Require(sigismember_nid_postfix(&currentBlocked, s) == 0);
    }

    for (const int how : {0, 4, 5, -1, 100}) {
        *__error_nid_postfix() = 0;
        Require(sigprocmask_nid_postfix(how, &toBlock, nullptr) == -1 && *__error_nid_postfix() == 22);
        *__error_nid_postfix() = 0;
        Require(_sigprocmask_nid_postfix(how, &toBlock, nullptr) == -1 && *__error_nid_postfix() == 22);

        *__error_nid_postfix() = 0;
        Require(sigprocmask_nid_postfix(how, nullptr, &currentBlocked) == 0);
        *__error_nid_postfix() = 0;
        Require(_sigprocmask_nid_postfix(how, nullptr, &currentBlocked) == 0);
    }

    Require(sigprocmask_nid_postfix(3, &initialBlocked, nullptr) == 0);
}

static void AuditPointerFaults() {
    constexpr std::size_t Page = 0x4000;
    void* mem = mmap_nid_postfix(nullptr, 3 * Page, 3, 0x1002, -1, 0);
    Require(mem != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    auto* unreadable = static_cast<unsigned char*>(mem);
    auto* readOnly = unreadable + Page;
    Require(mprotect_nid_postfix(unreadable, Page, 0) == 0);
    Require(mprotect_nid_postfix(readOnly, Page, 1) == 0);

    const auto* badAddress = reinterpret_cast<const void*>(std::uintptr_t{0x10});

    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(30, reinterpret_cast<const GuestSigaction*>(badAddress), nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(30, reinterpret_cast<const GuestSigaction*>(unreadable), nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(30, nullptr, const_cast<GuestSigaction*>(reinterpret_cast<const GuestSigaction*>(badAddress))) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(30, nullptr, reinterpret_cast<GuestSigaction*>(unreadable)) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigaction_nid_postfix(30, nullptr, reinterpret_cast<GuestSigaction*>(readOnly)) == -1 && *__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(sigprocmask_nid_postfix(1, badAddress, nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigprocmask_nid_postfix(1, unreadable, nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigprocmask_nid_postfix(1, nullptr, const_cast<void*>(badAddress)) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigprocmask_nid_postfix(1, nullptr, unreadable) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigprocmask_nid_postfix(1, nullptr, readOnly) == -1 && *__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(sigemptyset_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigemptyset_nid_postfix(const_cast<GuestSignalSet*>(reinterpret_cast<const GuestSignalSet*>(badAddress))) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigemptyset_nid_postfix(reinterpret_cast<GuestSignalSet*>(unreadable)) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigemptyset_nid_postfix(reinterpret_cast<GuestSignalSet*>(readOnly)) == -1 && *__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(sigfillset_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigfillset_nid_postfix(const_cast<GuestSignalSet*>(reinterpret_cast<const GuestSignalSet*>(badAddress))) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigfillset_nid_postfix(reinterpret_cast<GuestSignalSet*>(unreadable)) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigfillset_nid_postfix(reinterpret_cast<GuestSignalSet*>(readOnly)) == -1 && *__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(sigaddset_nid_postfix(nullptr, 1) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigaddset_nid_postfix(nullptr, 0) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(sigaddset_nid_postfix(const_cast<GuestSignalSet*>(reinterpret_cast<const GuestSignalSet*>(badAddress)), 1) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigaddset_nid_postfix(reinterpret_cast<GuestSignalSet*>(unreadable), 1) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigaddset_nid_postfix(reinterpret_cast<GuestSignalSet*>(readOnly), 1) == -1 && *__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(sigdelset_nid_postfix(nullptr, 1) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigdelset_nid_postfix(nullptr, 0) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(sigdelset_nid_postfix(const_cast<GuestSignalSet*>(reinterpret_cast<const GuestSignalSet*>(badAddress)), 1) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigdelset_nid_postfix(reinterpret_cast<GuestSignalSet*>(unreadable), 1) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigdelset_nid_postfix(reinterpret_cast<GuestSignalSet*>(readOnly), 1) == -1 && *__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    Require(sigismember_nid_postfix(nullptr, 1) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigismember_nid_postfix(nullptr, 0) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(sigismember_nid_postfix(reinterpret_cast<const GuestSignalSet*>(badAddress), 1) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigismember_nid_postfix(reinterpret_cast<const GuestSignalSet*>(unreadable), 1) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sigismember_nid_postfix(reinterpret_cast<const GuestSignalSet*>(readOnly), 1) >= 0 && *__error_nid_postfix() == 0);

    Require(mprotect_nid_postfix(mem, 3 * Page, 3) == 0);
    Require(munmap_nid_postfix(mem, 3 * Page) == 0);
}

int main() {
    AuditSigactionUncatchableAndBounds();
    AuditSigsetOperations();
    AuditSigprocmaskBoundariesAndUncatchable();
    AuditPointerFaults();
    return 0;
}
