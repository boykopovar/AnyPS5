#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <type_traits>

using SetKey = int (APS5_VABI*)(LibreSsl3Rsa*, LibreSsl3Bignum*, LibreSsl3Bignum*, LibreSsl3Bignum*);
using SetFactors = int (APS5_VABI*)(LibreSsl3Rsa*, LibreSsl3Bignum*, LibreSsl3Bignum*);
using ClearFree = void (APS5_VABI*)(LibreSsl3Bignum*);
static_assert(std::is_same_v<decltype(&RSA_set0_key_nid_postfix), SetKey>);
static_assert(std::is_same_v<decltype(&RSA_set0_crt_params_nid_postfix), SetKey>);
static_assert(std::is_same_v<decltype(&RSA_set0_factors_nid_postfix), SetFactors>);
static_assert(std::is_same_v<decltype(&BN_clear_free_nid_postfix), ClearFree>);

namespace {

std::size_t checks = 0, cases = 0, frees = 0;
struct Allocation { void* pointer; std::size_t size; };
std::array<Allocation, 32> allocations{};

void Require(bool condition, std::source_location where = std::source_location::current()) {
    ++checks;
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 RSA ownership check failed at line %u\n", where.line());
    std::abort();
}

bool IsLive(const void* pointer) {
    if (!pointer) return false;
    for (const auto& allocation : allocations)
        if (allocation.pointer == pointer) return true;
    return false;
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t bytes) {
    void* result = std::malloc(bytes + 16);
    Require(result != nullptr);
    std::memset(result, 0xa5, bytes + 16);
    for (auto& allocation : allocations) {
        if (allocation.pointer) continue;
        allocation = {result, bytes};
        return result;
    }
    std::abort();
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    for (auto& allocation : allocations) {
        if (allocation.pointer != pointer) continue;
        const auto* bytes = static_cast<const unsigned char*>(pointer);
        for (std::size_t i = 0; i < allocation.size; ++i) Require(bytes[i] == 0);
        for (std::size_t i = allocation.size; i < allocation.size + 16; ++i) Require(bytes[i] == 0xa5);
        allocation = {};
        ++frees;
        std::free(pointer);
        return;
    }
    Require(false);
}

void* APS5_VABI NoCalloc_nid_no_patch(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI NoRealloc_nid_no_patch(void*, std::size_t) { std::abort(); }
void* APS5_VABI NoAlign_nid_no_patch(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI NoRealign_nid_no_patch(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI NoPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { std::abort(); }

LibreSsl3Bignum* Number(int capacity) {
    auto* number = static_cast<LibreSsl3Bignum*>(malloc_nid_postfix(sizeof(LibreSsl3Bignum)));
    Require(number != nullptr);
    auto* data = static_cast<std::uint64_t*>(malloc_nid_postfix(capacity * sizeof(std::uint64_t)));
    Require(data != nullptr);
    for (int i = 0; i < capacity; ++i) data[i] = 0xfedcba9876543210ULL - i;
    *number = {data, capacity ? 1 : 0, capacity, 1, 1 | 4};
    return number;
}

void Clean() {
    for (const auto& allocation : allocations) Require(allocation.pointer == nullptr);
}

template<typename TCall> void Throws(TCall call) {
    bool threw = false;
    try { call(); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

using Field = void* LibreSsl3Rsa::*;
void Matrix(int group) {
    const std::array<Field, 3> fields = group == 0
        ? std::array<Field, 3>{&LibreSsl3Rsa::n, &LibreSsl3Rsa::e, &LibreSsl3Rsa::d}
        : group == 1 ? std::array<Field, 3>{&LibreSsl3Rsa::p, &LibreSsl3Rsa::q, nullptr}
        : std::array<Field, 3>{&LibreSsl3Rsa::dmp1, &LibreSsl3Rsa::dmq1, &LibreSsl3Rsa::iqmp};
    const int count = group == 1 ? 2 : 3;
    const int required = group == 2 ? 7 : 3;
    for (int oldMask = 0; oldMask < (1 << count); ++oldMask) {
        for (int newMask = 0; newMask < (1 << count); ++newMask) {
            ++cases;
            struct Guarded { std::uint64_t before; LibreSsl3Rsa key; std::uint64_t after; } guarded;
            std::memset(&guarded, 0xa5, sizeof(guarded));
            std::array<LibreSsl3Bignum*, 3> old{}, next{};
            std::array<LibreSsl3Bignum, 3> input{};
            for (int i = 0; i < count; ++i) {
                if (oldMask & (1 << i)) old[i] = Number(i + 2);
                if (newMask & (1 << i)) { next[i] = Number(i + 1); input[i] = *next[i]; }
                guarded.key.*fields[i] = old[i];
            }
            Guarded expected;
            std::memcpy(&expected, &guarded, sizeof(expected));
            const bool accepted = ((oldMask | newMask) & required) == required;
            const auto before = frees;
            std::size_t expectedFrees = 0;
            if (accepted) {
                for (int i = 0; i < count; ++i) {
                    if (!next[i]) continue;
                    expected.key.*fields[i] = next[i];
                    if (old[i]) expectedFrees += 2;
                }
            }
            const int result = group == 0 ? RSA_set0_key_nid_postfix(&guarded.key, next[0], next[1], next[2])
                : group == 1 ? RSA_set0_factors_nid_postfix(&guarded.key, next[0], next[1])
                : RSA_set0_crt_params_nid_postfix(&guarded.key, next[0], next[1], next[2]);
            Require(result == int(accepted));
            Require(std::memcmp(&guarded, &expected, sizeof(guarded)) == 0);
            Require(frees - before == expectedFrees);
            for (int i = 0; i < count; ++i) {
                if (next[i]) {
                    Require(IsLive(next[i]));
                    Require(std::memcmp(next[i], &input[i], sizeof(input[i])) == 0);
                    Require(next[i]->data[0] == 0xfedcba9876543210ULL);
                }
                if (IsLive(old[i])) BN_clear_free_nid_postfix(old[i]);
                BN_clear_free_nid_postfix(next[i]);
            }
            Clean();
        }
    }
}

void Disposal() {
    BN_clear_free_nid_postfix(nullptr);
    for (int capacity : {0, 1, 7}) {
        BN_clear_free_nid_postfix(Number(capacity));
        Clean();
    }
    for (int flags : {0, 2, 4, 6}) {
        std::array<std::uint64_t, 4> borrowed{11, 22, 33, 44};
        const auto expected = borrowed;
        const bool isStatic = (flags & 2) != 0;
        LibreSsl3Bignum number{isStatic ? borrowed.data() : static_cast<std::uint64_t*>(malloc_nid_postfix(sizeof(borrowed))), 1, 4, 1, flags};
        BN_clear_free_nid_postfix(&number);
        const LibreSsl3Bignum zero{};
        Require(std::memcmp(&number, &zero, sizeof(zero)) == 0);
        Require(borrowed == expected);
        Clean();
    }
    std::array<std::uint64_t, 2> borrowed{0x12345678, 0xabcdef};
    auto* number = static_cast<LibreSsl3Bignum*>(malloc_nid_postfix(sizeof(LibreSsl3Bignum)));
    *number = {borrowed.data(), 2, -1, 1, 3};
    BN_clear_free_nid_postfix(number);
    Require(borrowed[0] == 0x12345678 && borrowed[1] == 0xabcdef);
    Clean();
    LibreSsl3Bignum empty{nullptr, -1, -1, 1, 0};
    BN_clear_free_nid_postfix(&empty);
    const LibreSsl3Bignum zero{};
    Require(std::memcmp(&empty, &zero, sizeof(zero)) == 0);
}

void Invalid() {
    Throws([] { RSA_set0_key_nid_postfix(nullptr, nullptr, nullptr, nullptr); });
    Throws([] { RSA_set0_factors_nid_postfix(nullptr, nullptr, nullptr); });
    Throws([] { RSA_set0_crt_params_nid_postfix(nullptr, nullptr, nullptr, nullptr); });
    std::uint64_t limb = 0x1234;
    LibreSsl3Bignum bad{&limb, 1, -1, 0, 0};
    const auto original = bad;
    Throws([&] { BN_clear_free_nid_postfix(&bad); });
    Require(std::memcmp(&bad, &original, sizeof(bad)) == 0 && limb == 0x1234);
    auto* old = Number(2);
    auto* first = Number(3);
    auto* second = Number(4);
    LibreSsl3Rsa rsa{};
    rsa.n = rsa.p = rsa.dmp1 = old;
    rsa.e = rsa.q = rsa.dmq1 = &bad;
    rsa.iqmp = &bad;
    const auto before = rsa;
    const auto beforeFrees = frees;
    Throws([&] { RSA_set0_key_nid_postfix(&rsa, first, second, nullptr); });
    Throws([&] { RSA_set0_factors_nid_postfix(&rsa, first, second); });
    Throws([&] { RSA_set0_crt_params_nid_postfix(&rsa, first, nullptr, second); });
    Require(std::memcmp(&rsa, &before, sizeof(rsa)) == 0 && frees == beforeFrees);
    BN_clear_free_nid_postfix(old);
    BN_clear_free_nid_postfix(first);
    BN_clear_free_nid_postfix(second);
    Clean();
}

}

int main() {
    void* const api[10]{reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(NoCalloc_nid_no_patch), reinterpret_cast<void*>(NoRealloc_nid_no_patch),
        reinterpret_cast<void*>(NoAlign_nid_no_patch), reinterpret_cast<void*>(NoRealign_nid_no_patch),
        reinterpret_cast<void*>(NoPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    Matrix(0);
    Matrix(1);
    Matrix(2);
    Disposal();
    Invalid();
    Clean();
    std::printf("RSA ownership: %zu matrix cases, %zu checks, %zu cleared allocations, zero live allocations\n", cases, checks, frees);
}
