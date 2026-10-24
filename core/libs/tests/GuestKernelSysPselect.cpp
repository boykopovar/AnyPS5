#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

extern "C" {
int* APS5_VABI __error_nid_postfix();
int APS5_VABI pipe_nid_postfix(int* descriptors);
int APS5_VABI close_nid_postfix(int descriptor);
std::int64_t APS5_VABI write_nid_postfix(int descriptor, const char* buffer, std::int64_t size);
int APS5_VABI sigprocmask_nid_postfix(int how, const void* set, void* previousSet);
int APS5_VABI pselect_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout, const void* sigmask);
}

namespace {

constexpr int Einval = 22;
constexpr int SigBlock = 1;
constexpr int SigSetmask = 3;
constexpr int SigUsr1 = 30;

struct Timespec {
    std::int64_t seconds;
    std::int64_t nanoseconds;
};

struct SignalSet {
    std::uint32_t bits[4]{};
};

struct DescriptorSet {
    std::array<std::uint64_t, 1024 / 64> words{};
    void Add(int descriptor) { words[descriptor / 64] |= std::uint64_t{1} << (descriptor % 64); }
    bool Has(int descriptor) const { return ((words[descriptor / 64] >> (descriptor % 64)) & 1u) != 0; }
};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

SignalSet CurrentMask() {
    SignalSet current{};
    Require(sigprocmask_nid_postfix(SigBlock, nullptr, &current) == 0);
    return current;
}

bool Same(const SignalSet& left, const SignalSet& right) {
    return std::memcmp(left.bits, right.bits, sizeof(left.bits)) == 0;
}

}

int main() {
    int descriptors[2]{-1, -1};
    Require(pipe_nid_postfix(descriptors) == 0);
    const int reader = descriptors[0];
    const int writer = descriptors[1];
    SignalSet blocked{};
    blocked.bits[(SigUsr1 - 1) / 32] |= 1u << ((SigUsr1 - 1) % 32);
    const SignalSet before = CurrentMask();
    {
        DescriptorSet read{};
        read.Add(reader);
        const Timespec limit{0, 200000000};
        const auto start = std::chrono::steady_clock::now();
        Require(pselect_nid_postfix(reader + 1, read.words.data(), nullptr, nullptr, &limit, &blocked) == 0);
        Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(150));
        Require(!read.Has(reader));
        Require(Same(CurrentMask(), before));
    }
    Require(write_nid_postfix(writer, "x", 1) == 1);
    {
        DescriptorSet read{};
        read.Add(reader);
        const Timespec limit{0, 999999999};
        Require(pselect_nid_postfix(reader + 1, read.words.data(), nullptr, nullptr, &limit, nullptr) == 1);
        Require(read.Has(reader));
    }
    {
        DescriptorSet read{};
        read.Add(reader);
        Require(pselect_nid_postfix(reader + 1, read.words.data(), nullptr, nullptr, nullptr, &blocked) == 1);
        Require(read.Has(reader));
        Require(Same(CurrentMask(), before));
    }
    {
        const Timespec invalid{0, 1000000000};
        Require(pselect_nid_postfix(0, nullptr, nullptr, nullptr, &invalid, &blocked) == -1 && Error() == Einval);
        Require(Same(CurrentMask(), before));
        const Timespec zero{0, 0};
        Require(pselect_nid_postfix(-1, nullptr, nullptr, nullptr, &zero, nullptr) == -1 && Error() == Einval);
    }
    Require(sigprocmask_nid_postfix(SigSetmask, &before, nullptr) == 0);
    Require(close_nid_postfix(reader) == 0 && close_nid_postfix(writer) == 0);
    std::puts("pselect passed");
    return 0;
}
