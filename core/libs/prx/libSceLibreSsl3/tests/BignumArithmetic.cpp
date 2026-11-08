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
#include <source_location>
#include <stdexcept>
#include <unordered_map>

namespace {

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

void Dup() {
    Require(BN_dup_nid_postfix(nullptr) == nullptr);

    std::array<std::uint64_t, 2> magnitude{0x0123456789abcdefULL, 0xfedcba9876543210ULL};
    Bignum source{};
    source.d = magnitude.data();
    source.top = 2;
    source.dmax = 2;
    source.neg = 1;
    Bignum* copy = BN_dup_nid_postfix(&source);
    Require(copy != nullptr);
    Require(copy->top == 2 && copy->dmax >= 2 && copy->neg == 1);
    Require(copy->d != source.d);
    Require(copy->d[0] == magnitude[0] && copy->d[1] == magnitude[1]);
    source.d[0] = 0;
    Require(copy->d[0] == 0x0123456789abcdefULL);
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
    Require(allocations.empty());
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
    std::puts("libSceLibreSsl3 BIGNUM tests passed");
}
