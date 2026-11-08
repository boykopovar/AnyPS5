#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

using Magnitude = std::vector<std::uint64_t>;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIGNUM check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> allocations;
int allocationsUntilFailure = -1;

bool FailAllocation() {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return true;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    return false;
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (FailAllocation()) return nullptr;
    void* result = std::malloc(size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) {
    if (FailAllocation()) return nullptr;
    void* result = std::calloc(count, size);
    Require(result != nullptr);
    allocations.emplace(result, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (FailAllocation()) return nullptr;
    if (!pointer) return Allocate_nid_no_patch(size);
    Require(allocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned allocation");
}

void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected aligned reallocation");
}

int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected posix_memalign");
}

void Trim(Magnitude& magnitude) {
    while (!magnitude.empty() && magnitude.back() == 0) magnitude.pop_back();
}

Magnitude Load(const Bignum& value) {
    Magnitude magnitude;
    if (value.top > 0) magnitude.assign(value.d, value.d + value.top);
    Trim(magnitude);
    return magnitude;
}

Bignum Input(const Magnitude& magnitude, bool negative = false) {
    std::size_t top = magnitude.size();
    while (top > 0 && magnitude[top - 1] == 0) --top;
    Bignum value{};
    value.d = const_cast<std::uint64_t*>(magnitude.data());
    value.top = static_cast<int>(top);
    value.dmax = static_cast<int>(magnitude.size());
    value.neg = negative ? 1 : 0;
    return value;
}

void Release(Bignum& value) {
    BN_free_nid_postfix(&value);
    value = {};
}

Magnitude Multiply(const Magnitude& first, const Magnitude& second) {
    if (first.empty() || second.empty()) return {};
    Magnitude result(first.size() + second.size(), 0);
    for (std::size_t i = 0; i < first.size(); ++i) {
        std::uint64_t carry = 0;
        for (std::size_t j = 0; j < second.size(); ++j) {
            const unsigned __int128 product = static_cast<unsigned __int128>(first[i]) * second[j] + result[i + j] + carry;
            result[i + j] = static_cast<std::uint64_t>(product);
            carry = static_cast<std::uint64_t>(product >> 64);
        }
        result[i + second.size()] = carry;
    }
    Trim(result);
    return result;
}

Magnitude Add(const Magnitude& first, const Magnitude& second) {
    const std::size_t count = first.size() > second.size() ? first.size() : second.size();
    Magnitude result(count + 1, 0);
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const unsigned __int128 sum = static_cast<unsigned __int128>(i < first.size() ? first[i] : 0) +
                                      (i < second.size() ? second[i] : 0) + carry;
        result[i] = static_cast<std::uint64_t>(sum);
        carry = static_cast<std::uint64_t>(sum >> 64);
    }
    result[count] = carry;
    Trim(result);
    return result;
}

int Compare(const Magnitude& first, const Magnitude& second) {
    if (first.size() != second.size()) return first.size() < second.size() ? -1 : 1;
    for (std::size_t index = first.size(); index-- > 0;) {
        if (first[index] != second[index]) return first[index] < second[index] ? -1 : 1;
    }
    return 0;
}

void RequireEqual(const Magnitude& actual, const Magnitude& expected) {
    Require(actual == expected);
}

std::uint64_t Next(std::uint64_t& state) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return state;
}

void Division(const Magnitude& numeratorInput, const Magnitude& denominatorInput, bool numeratorNegative, bool denominatorNegative) {
    Magnitude numerator = numeratorInput;
    Magnitude denominator = denominatorInput;
    Trim(numerator);
    Trim(denominator);
    Bignum dividend = Input(numerator, numeratorNegative);
    Bignum divisor = Input(denominator, denominatorNegative);
    Bignum quotient{};
    Bignum remainder{};
    const int result = BN_div_nid_postfix(&quotient, &remainder, &dividend, &divisor, nullptr);
    if (result != 1) std::fprintf(stderr, "BN_div returned %d for %zu/%zu limbs\n", result, numerator.size(), denominator.size());
    Require(result == 1);
    const Magnitude quotientMagnitude = Load(quotient);
    const Magnitude remainderMagnitude = Load(remainder);
    Require(Compare(remainderMagnitude, denominator) < 0);
    const Magnitude combined = Add(Multiply(quotientMagnitude, denominator), remainderMagnitude);
    if (combined != numerator) {
        std::fprintf(stderr, "BN_div invariant failed: numerator=%zu denominator=%zu quotient=%zu remainder=%zu combined=%zu\n",
                     numerator.size(), denominator.size(), quotientMagnitude.size(), remainderMagnitude.size(), combined.size());
        std::fprintf(stderr, "n[0]=%llx d[0]=%llx q[0]=%llx r[0]=%llx c[0]=%llx\n",
                     static_cast<unsigned long long>(numerator.empty() ? 0 : numerator[0]),
                     static_cast<unsigned long long>(denominator.empty() ? 0 : denominator[0]),
                     static_cast<unsigned long long>(quotientMagnitude.empty() ? 0 : quotientMagnitude[0]),
                     static_cast<unsigned long long>(remainderMagnitude.empty() ? 0 : remainderMagnitude[0]),
                     static_cast<unsigned long long>(combined.empty() ? 0 : combined[0]));
    }
    RequireEqual(combined, numerator);
    Require(quotient.neg == ((numeratorNegative != denominatorNegative) && !quotientMagnitude.empty() ? 1 : 0));
    Require(remainder.neg == (numeratorNegative && !remainderMagnitude.empty() ? 1 : 0));
    Release(quotient);
    Release(remainder);
}

void Dup() {
    Require(BN_dup_nid_postfix(nullptr) == nullptr);
    Magnitude magnitude{0x0123456789abcdefULL, 0xfedcba9876543210ULL};
    const std::uint64_t first = magnitude[0];
    Bignum source = Input(magnitude, true);
    Bignum* copy = BN_dup_nid_postfix(&source);
    Require(copy != nullptr);
    Require(copy->top == 2 && copy->dmax >= 2 && copy->neg == 1);
    Require(copy->d[0] == magnitude[0] && copy->d[1] == magnitude[1]);
    source.d[0] = 0;
    Require(copy->d[0] == first);
    BN_free_nid_postfix(copy);

    Bignum zero{};
    Bignum* zeroCopy = BN_dup_nid_postfix(&zero);
    Require(zeroCopy != nullptr && zeroCopy->top == 0 && zeroCopy->neg == 0);
    BN_free_nid_postfix(zeroCopy);

    allocationsUntilFailure = 0;
    Require(BN_dup_nid_postfix(&source) == nullptr);
    allocationsUntilFailure = 1;
    Require(BN_dup_nid_postfix(&source) == nullptr);
    allocationsUntilFailure = -1;
}

void SmallDivision() {
    Division({100}, {7}, false, false);
    Division({100}, {7}, false, true);
    Division({100}, {7}, true, false);
    Division({100}, {7}, true, true);
    Division({7}, {100}, false, false);
    Division({0}, {100}, false, false);
    Division({0xffffffffffffffffULL, 0xffffffffffffffffULL}, {0xffffffffffffffffULL}, false, false);
    Division({1, 2, 3, 4}, {5, 6}, false, false);
    Division({1, 2, 3, 4}, {5, 6}, true, true);
}

void Errors() {
    const Magnitude hundred{100};
    const Magnitude zero{0};
    const Magnitude seven{7};
    const Magnitude invalidMagnitude{0, 1};
    Bignum dividend = Input(hundred);
    Bignum divisor = Input(zero);
    Bignum quotient{};
    Bignum remainder{};
    Require(BN_div_nid_postfix(&quotient, &remainder, &dividend, &divisor, nullptr) == 0);
    Require(ERR_get_error_nid_postfix() == 0x03fff067);
    Release(quotient);
    Release(remainder);

    Bignum invalid = Input(invalidMagnitude);
    invalid.top = 2;
    invalid.d[1] = 0;
    Bignum valid = Input(seven);
    Require(BN_div_nid_postfix(&quotient, &remainder, &invalid, &valid, nullptr) == 0);
    Require(ERR_get_error_nid_postfix() == 0x03fff06b);
    Release(quotient);
    Release(remainder);

    bool threw = false;
    try {
        BN_div_nid_postfix(&quotient, &remainder, nullptr, &valid, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        BN_div_nid_postfix(&quotient, &remainder, &valid, nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void Aliases() {
    Magnitude numerator{100};
    const Magnitude seven{7};
    Bignum value = Input(numerator);
    value.flags = 2;
    Bignum divisor = Input(seven);
    Bignum remainder{};
    Require(BN_div_nid_postfix(&value, &remainder, &value, &divisor, nullptr) == 1);
    Require(value.top == 1 && value.d[0] == 14 && value.neg == 0);
    Require(remainder.top == 1 && remainder.d[0] == 2 && remainder.neg == 0);
    Release(value);
    Release(remainder);

    Require(BN_div_nid_postfix(nullptr, nullptr, &divisor, &divisor, nullptr) == 1);
}

void RandomDivision() {
    std::uint64_t state = 0x123456789abcdef0ULL;
    for (int iteration = 0; iteration < 200; ++iteration) {
        Magnitude numerator(Next(state) % 7 + 1);
        Magnitude denominator(Next(state) % 4 + 1);
        for (auto& limb : numerator) limb = Next(state);
        for (auto& limb : denominator) limb = Next(state);
        Trim(numerator);
        Trim(denominator);
        if (denominator.empty()) denominator.push_back(1);
        if (!numerator.empty()) numerator.back() |= 1;
        Division(numerator, denominator, (Next(state) & 1) != 0, (Next(state) & 1) != 0);
    }
}

void AllocationFailure() {
    Magnitude numerator{100};
    Magnitude denominator{7};
    Bignum dividend = Input(numerator);
    Bignum divisor = Input(denominator);
    Bignum quotient{};
    Bignum remainder{};
    allocationsUntilFailure = 0;
    Require(BN_div_nid_postfix(&quotient, &remainder, &dividend, &divisor, nullptr) == 0);
    allocationsUntilFailure = -1;
    Release(quotient);
    Release(remainder);
}

}

int main() {
    std::array<void*, 7> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Dup();
    SmallDivision();
    Errors();
    Aliases();
    RandomDivision();
    AllocationFailure();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 BIGNUM arithmetic tests passed");
}
