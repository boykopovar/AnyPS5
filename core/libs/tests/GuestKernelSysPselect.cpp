#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr int Eintr = 4;
constexpr int Efault = 14;
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

int caught = 0;

void APS5_VABI Count(int) {
    ++caught;
}

void RaiseBlocked(const SignalSet& blocked) {
    Require(sigprocmask_nid_postfix(SigBlock, &blocked, nullptr) == 0);
    Require(raise_nid_postfix(SigUsr1) == 0 && caught == 0);
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
        const auto* unmapped = reinterpret_cast<const void*>(std::uintptr_t{0x10});
        Require(pselect_nid_postfix(0, nullptr, nullptr, nullptr, unmapped, &blocked) == -1 && Error() == Efault);
        Require(Same(CurrentMask(), before));
        Require(pselect_nid_postfix(0, nullptr, nullptr, nullptr, &zero, unmapped) == -1 && Error() == Efault);
        Require(Same(CurrentMask(), before));
    }
    Require(signal_nid_postfix(SigUsr1, Count) != reinterpret_cast<GuestHandler>(static_cast<std::uintptr_t>(-1)));
    const SignalSet empty{};
    {
        RaiseBlocked(blocked);
        const SignalSet withUsr1 = CurrentMask();
        DescriptorSet read{};
        read.Add(writer);
        const DescriptorSet original = read;
        const Timespec limit{0, 300000000};
        const auto start = std::chrono::steady_clock::now();
        Require(pselect_nid_postfix(writer + 1, read.words.data(), nullptr, nullptr, &limit, &empty) == -1 && Error() == Eintr);
        Require(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(150));
        Require(caught == 1);
        Require(read.words == original.words);
        Require(Same(CurrentMask(), withUsr1));
        caught = 0;
    }
    {
        RaiseBlocked(blocked);
        Require(pselect_nid_postfix(0, nullptr, nullptr, nullptr, nullptr, &empty) == -1 && Error() == Eintr && caught == 1);
        caught = 0;
    }
    {
        RaiseBlocked(blocked);
        const Timespec limit{0, 300000000};
        Require(pselect_nid_postfix(1, reinterpret_cast<void*>(std::uintptr_t{0x10}), nullptr, nullptr, &limit, &empty) == -1 && Error() == Efault && caught == 1);
        caught = 0;
    }
    {
        RaiseBlocked(blocked);
        DescriptorSet read{};
        read.Add(reader);
        const Timespec limit{0, 300000000};
        Require(pselect_nid_postfix(reader + 1, read.words.data(), nullptr, nullptr, &limit, &empty) == 1 && caught == 1);
        Require(read.Has(reader));
        caught = 0;
    }
    {
        RaiseBlocked(blocked);
        const Timespec zero{0, 0};
        DescriptorSet read{};
        read.Add(writer);
        Require(pselect_nid_postfix(writer + 1, read.words.data(), nullptr, nullptr, &zero, &empty) == 0 && caught == 1);
        caught = 0;
    }
    {
        RaiseBlocked(blocked);
        const Timespec invalid{0, 1000000000};
        Require(pselect_nid_postfix(0, nullptr, nullptr, nullptr, &invalid, &empty) == -1 && Error() == Einval && caught == 1);
        caught = 0;
    }
    {
        RaiseBlocked(blocked);
        const Timespec limit{0, 200000000};
        const auto start = std::chrono::steady_clock::now();
        Require(pselect_nid_postfix(0, nullptr, nullptr, nullptr, &limit, &blocked) == 0 && caught == 0);
        Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(150));
        Require(sigprocmask_nid_postfix(SigSetmask, &empty, nullptr) == 0 && caught == 1);
        caught = 0;
    }
    Require(sigprocmask_nid_postfix(SigSetmask, &before, nullptr) == 0);
    Require(close_nid_postfix(reader) == 0 && close_nid_postfix(writer) == 0);
    std::puts("pselect passed");
    return 0;
}
