#include "prx/libc/libc.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "sigvec: %s\n", message);
    std::abort();
}

GuestSignalSet Mask() {
    GuestSignalSet value{};
    Require(_sigprocmask_nid_postfix(0, nullptr, &value) == 0, "query mask");
    return value;
}

bool Equal(const GuestSignalSet& first, const GuestSignalSet& second) {
    for (int word = 0; word < 4; ++word) if (first.bits[word] != second.bits[word]) return false;
    return true;
}

bool Equal(const GuestSignalVector& first, const GuestSignalVector& second) {
    return first.handler == second.handler && first.mask == second.mask && first.flags == second.flags;
}

int received = 0;
int signalReceived = 0;

void APS5_VABI Handler(int signal) {
    ++received;
    signalReceived = signal;
}

void Vectors() {
    constexpr int signal = 30;
    const auto handler = reinterpret_cast<std::uintptr_t>(&Handler);
    GuestSigaction initial{};
    Require(sigaction_nid_postfix(signal, nullptr, &initial) == 0, "save original action");
    for (int flags = 0; flags < 128; ++flags) {
        const GuestSignalVector replacement{handler, std::bit_cast<std::int32_t>(0x80000005u), flags};
        Require(sigvec_nid_disambig1(signal, &replacement, nullptr) == 0, "install each supported flag combination");
        GuestSigaction actual{};
        Require(sigaction_nid_postfix(signal, nullptr, &actual) == 0, "query modern action");
        Require(actual.handler == handler && actual.flags == (flags ^ 2), "legacy interrupt flag reverses restart flag");
        Require(Equal(actual.mask, GuestSignalSet{{0x80000005, 0, 0, 0}}), "legacy vector zero extends mask");
        GuestSignalVector queried{};
        Require(sigvec_nid_disambig1(signal, nullptr, &queried) == 0 && Equal(queried, replacement), "query converts flags and mask back");
    }
    const GuestSigaction extended{handler, 0x13, {{0x87654321, 0x12345678, 0x80000001, 0x7fffffff}}};
    Require(sigaction_nid_postfix(signal, &extended, nullptr) == 0, "install action with extended mask");
    GuestSignalVector queried{};
    *__error_nid_postfix() = 81;
    Require(sigvec_nid_disambig1(signal, nullptr, &queried) == 0, "query extended action");
    Require(Equal(queried, GuestSignalVector{handler, std::bit_cast<std::int32_t>(0x87654321u), 0x11}), "query truncates modern mask to low word");
    Require(*__error_nid_postfix() == 81, "successful vector preserves errno");
    GuestSignalVector alias{handler, 7, 0};
    Require(sigvec_nid_disambig1(signal, &alias, &alias) == 0 && Equal(alias, queried), "aliased input and output returns old action");
    GuestSignalVector newState{};
    Require(sigvec_nid_disambig1(signal, nullptr, &newState) == 0 && Equal(newState, GuestSignalVector{handler, 7, 0}), "alias installs original input");
    const GuestSignalVector sentinel{0x12345678, -123, 0x345};
    const GuestSignalVector invalid{handler, -1, 0x80};
    for (const int number : std::array{-1, 0, 129, signal}) {
        GuestSignalVector output = sentinel;
        *__error_nid_postfix() = 0;
        Require(sigvec_nid_disambig1(number, &invalid, &output) == -1, "invalid signal or flags fail");
        Require(*__error_nid_postfix() == 22 && Equal(output, sentinel), "failure preserves previous output and exposes errno");
    }
    Require(sigvec_nid_disambig1(signal, nullptr, &queried) == 0 && Equal(queried, newState), "failed update leaves disposition unchanged");
    for (const int number : std::array{9, 17}) {
        GuestSignalVector output = sentinel;
        Require(sigvec_nid_disambig1(number, &newState, &output) == -1 && Equal(output, sentinel), "uninstallable handlers preserve output");
    }
    Require(sigvec_nid_disambig1(signal, nullptr, nullptr) == 0, "null input and output query is accepted");
    const GuestSignalSet saved = Mask();
    const GuestSignalSet empty{};
    Require(_sigprocmask_nid_postfix(3, &empty, nullptr) == 0, "clear mask for delivery");
    received = 0;
    const GuestSignalSet blocked{{1u << (signal - 1), 0, 0, 0}};
    Require(_sigprocmask_nid_postfix(1, &blocked, nullptr) == 0, "block installed handler");
    Require(raise_nid_postfix(signal) == 0 && received == 0, "blocked signal becomes pending");
    GuestSignalSet previous{};
    Require(_sigprocmask_nid_postfix(3, &empty, &previous) == 0 && previous.bits[0] == (1u << (signal - 1)), "unblock returns prior mask");
    Require(received == 1 && signalReceived == signal, "unblock delivers pending signal through guest handler");
    Require(raise_nid_postfix(signal) == 0 && received == 2, "unblocked delivery retains vector handler");
    Require(sigaction_nid_postfix(signal, &initial, nullptr) == 0, "restore disposition");
    Require(_sigprocmask_nid_postfix(3, &saved, nullptr) == 0, "restore mask after delivery");
}

void GuestBackedOutput() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t bytes = page * 2;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0, "allocate guest signal storage");
    void* mapping = nullptr;
    Require(sceKernelMapDirectMemory(&mapping, bytes, 3, 0, physical, 0) == 0 && mapping != nullptr, "map guest signal storage");
    Require(GuestArena::GuestArenaContains_nid_postfix(mapping, bytes), "output is not backed by the guest arena");
    auto* memory = static_cast<unsigned char*>(mapping);
    {
        GuestArena::HostWrite access(mapping, bytes);
        Require(access.Open(), "initialize guest signal storage");
        std::memset(mapping, 0xA5, bytes);
    }
    constexpr int signal = 30;
    GuestSigaction initial{};
    Require(sigaction_nid_postfix(signal, nullptr, &initial) == 0, "save guest-output disposition");
    const GuestSignalVector replacement{reinterpret_cast<std::uintptr_t>(&Handler), -123, 0};
    auto* output = reinterpret_cast<GuestSignalVector*>(memory + page - 8);
    Require(sigvec_nid_disambig1(signal, &replacement, output) == 0, "write complete vector across guest pages");
    Require(Equal(*output, GuestSignalVector{initial.handler, std::bit_cast<std::int32_t>(initial.mask.bits[0]), initial.flags ^ 2}), "guest vector conversion");
    Require(memory[page - 9] == 0xA5 && memory[page + 8] == 0xA5, "guest vector output escaped its bounds");
    *output = replacement;
    Require(sigvec_nid_disambig1(signal, output, output) == 0 && Equal(*output, replacement), "guest input-output alias conversion");
#ifdef _WIN32
    const GuestSignalVector sentinel{0x12345678, -123, 0x345};
    const GuestSignalVector changed{replacement.handler, 5, 2};
    const auto reject = [&](GuestSignalVector* destination, void* protectedPage) {
        *destination = sentinel;
        Require(sceKernelMprotect(protectedPage, page, 1) == 0, "protect guest vector output");
        {
            GuestArena::HostWrite access(destination, sizeof(*destination));
            Require(!access.Open(), "read-only guest output unexpectedly opened");
        }
        *__error_nid_postfix() = 73;
        Require(sigvec_nid_disambig1(signal, &changed, destination) == -1 && *__error_nid_postfix() == 14, "read-only guest vector was not rejected");
        Require(Equal(*destination, sentinel), "rejected guest vector was partially overwritten");
        GuestSignalVector current{};
        Require(sigvec_nid_disambig1(signal, nullptr, &current) == 0 && Equal(current, replacement), "rejected output changed the disposition");
        Require(sigvec_nid_disambig1(signal, destination, destination) == -1 && *__error_nid_postfix() == 14, "read-only aliased vector was not rejected before mutation");
        Require(Equal(*destination, sentinel), "rejected aliased vector was changed");
        Require(sigvec_nid_disambig1(signal, nullptr, &current) == 0 && Equal(current, replacement), "rejected alias changed the disposition");
        Require(sceKernelMprotect(protectedPage, page, 3) == 0, "restore guest vector protection");
    };
    reject(reinterpret_cast<GuestSignalVector*>(memory + 128), mapping);
    reject(output, memory + page);
#endif
    Require(sigaction_nid_postfix(signal, &initial, nullptr) == 0, "restore guest-output disposition");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), bytes) == 0, "unmap guest signal storage");
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0, "release guest signal storage");
}

void MissingBackend() {
    const auto saved = Mask();
    const GuestSignalVector sentinel{0x12345678, -123, 0x345};
    GuestSignalVector output = sentinel;
    const GuestSignalVector replacement{reinterpret_cast<std::uintptr_t>(&Handler), 7, 0};
    RegisterSigvec_nid_no_patch(nullptr);
    bool threw = false;
    try {
        sigvec_nid_disambig1(30, &replacement, &output);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "missing backend throws");
    Require(Equal(output, sentinel) && Equal(Mask(), saved), "missing backend leaves output and masks untouched");
    RegisterSigvec_nid_no_patch(sigaction_nid_postfix);
}

}

int main() {
    Vectors();
    GuestBackedOutput();
    MissingBackend();
}
