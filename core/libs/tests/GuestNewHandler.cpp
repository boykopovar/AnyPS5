#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

extern "C" {
using Handler = void (APS5_VABI*)();

}

namespace {

alignas(16) std::array<std::byte, 64> storage{};
unsigned failuresLeft = 0;
unsigned allocations = 0;
unsigned releases = 0;
unsigned handlerCalls = 0;
std::size_t lastSize = 0;
std::size_t lastAlignment = 0;

void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Guest new handler check failed at line %d\n", line);
        std::abort();
    }
}

#define REQUIRE(condition) Require((condition), __LINE__)

void* APS5_VABI allocate(std::size_t bytes) {
    ++allocations;
    lastSize = bytes;
    if (failuresLeft != 0) {
        --failuresLeft;
        return nullptr;
    }
    return storage.data();
}

void APS5_VABI release(void*) { ++releases; }
void* APS5_VABI allocateZeroed(std::size_t count, std::size_t bytes) { return allocate(count * bytes); }
void* APS5_VABI reallocate(void*, std::size_t bytes) { return allocate(bytes); }
void* APS5_VABI align(std::size_t alignment, std::size_t bytes) {
    lastAlignment = alignment;
    return allocate(bytes);
}
void* APS5_VABI realign(void*, std::size_t bytes, std::size_t) { return allocate(bytes); }
int APS5_VABI posixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = allocate(bytes);
    return *pointer == nullptr ? 12 : 0;
}

void APS5_VABI countingHandler() { ++handlerCalls; }
void APS5_VABI uninstallingHandler() {
    if (++handlerCalls == 3) _ZSt15set_new_handlerPFvvE_nid_postfix(nullptr);
}
void APS5_VABI throwingHandler() {
    ++handlerCalls;
    throw std::bad_alloc();
}
void APS5_VABI allocatingHandler() {
    ++handlerCalls;
    failuresLeft = 0;
    REQUIRE(_Znwm_nid_postfix(8) == storage.data());
}

void Reset(unsigned failures) {
    failuresLeft = failures;
    allocations = 0;
    releases = 0;
    handlerCalls = 0;
}

template<typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::bad_alloc&) {
        return true;
    }
    return false;
}

}

int RunDefaultHeap() {
    std::array<void*, 10> none{};
    ApplicationHeapRegister_nid_no_patch(none.data());
    constexpr std::size_t huge = std::numeric_limits<std::size_t>::max();

    REQUIRE(Throws([] { _Znwm_nid_postfix(huge); }));
    REQUIRE(Throws([] { _Znam_nid_postfix(huge); }));
    REQUIRE(_ZnwmRKSt9nothrow_t_nid_postfix(huge, nullptr) == nullptr);
    REQUIRE(_ZnamRKSt9nothrow_t_nid_postfix(huge, nullptr) == nullptr);

    handlerCalls = 0;
    REQUIRE(_ZSt15set_new_handlerPFvvE_nid_postfix(&uninstallingHandler) == nullptr);
    REQUIRE(Throws([] { _Znwm_nid_postfix(huge); }));
    REQUIRE(handlerCalls == 3 && _ZSt15get_new_handlerv_nid_postfix() == nullptr);

    handlerCalls = 0;
    REQUIRE(_ZSt15set_new_handlerPFvvE_nid_postfix(&throwingHandler) == nullptr);
    REQUIRE(_ZnwmRKSt9nothrow_t_nid_postfix(huge, nullptr) == nullptr && handlerCalls == 1);
    REQUIRE(_ZSt15set_new_handlerPFvvE_nid_postfix(nullptr) == &throwingHandler);

    void* small = _Znwm_nid_postfix(24);
    REQUIRE(small != nullptr);
    std::memset(small, 0x5a, 24);
    _ZdlPv_nid_postfix(small);

    std::puts("Guest new handler default heap checks passed");
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "default") == 0) return RunDefaultHeap();
    std::array<void*, 10> api{};
    api[0] = reinterpret_cast<void*>(&allocate);
    api[1] = reinterpret_cast<void*>(&release);
    api[2] = reinterpret_cast<void*>(&allocateZeroed);
    api[3] = reinterpret_cast<void*>(&reallocate);
    api[4] = reinterpret_cast<void*>(&align);
    api[5] = reinterpret_cast<void*>(&realign);
    api[6] = reinterpret_cast<void*>(&posixAlign);
    ApplicationHeapRegister_nid_no_patch(api.data());

    REQUIRE(_ZSt15get_new_handlerv_nid_postfix() == nullptr);

    Reset(0);
    void* nothrowPointer = _Znwm_nid_postfix(8);
    const int nothrowTag = 0;
    REQUIRE(nothrowPointer == storage.data() && releases == 0);
    _ZdlPvRKSt9nothrow_t_nid_postfix(nothrowPointer, &nothrowTag);

    REQUIRE(releases == 1);
    _ZdlPvRKSt9nothrow_t_nid_postfix(nullptr, &nothrowTag);

    REQUIRE(releases == 1);

    Reset(1);
    REQUIRE(Throws([] { _Znwm_nid_postfix(8); }) && handlerCalls == 0 && allocations == 1);
    Reset(1);
    REQUIRE(Throws([] { _Znam_nid_postfix(8); }) && handlerCalls == 0 && allocations == 1);
    Reset(1);
    REQUIRE(_ZnwmRKSt9nothrow_t_nid_postfix(8, nullptr) == nullptr && handlerCalls == 0);
    Reset(1);
    REQUIRE(_ZnamRKSt9nothrow_t_nid_postfix(8, nullptr) == nullptr && handlerCalls == 0);

    REQUIRE(_ZSt15set_new_handlerPFvvE_nid_postfix(&countingHandler) == nullptr);
    REQUIRE(_ZSt15get_new_handlerv_nid_postfix() == &countingHandler);
    Reset(2);
    REQUIRE(_Znwm_nid_postfix(8) == storage.data() && handlerCalls == 2 && allocations == 3);
    Reset(3);
    REQUIRE(_Znam_nid_postfix(8) == storage.data() && handlerCalls == 3 && allocations == 4);
    Reset(1);
    REQUIRE(_ZnwmRKSt9nothrow_t_nid_postfix(8, nullptr) == storage.data() && handlerCalls == 1 && allocations == 2);
    Reset(1);
    REQUIRE(_ZnamRKSt9nothrow_t_nid_postfix(8, nullptr) == storage.data() && handlerCalls == 1 && allocations == 2);
    Reset(0);
    REQUIRE(_Znwm_nid_postfix(8) == storage.data() && handlerCalls == 0 && allocations == 1);
    Reset(1);
    REQUIRE(_Znwm_nid_postfix(0) == storage.data() && lastSize == 1 && handlerCalls == 1);

    REQUIRE(_ZSt15set_new_handlerPFvvE_nid_postfix(&uninstallingHandler) == &countingHandler);
    Reset(100);
    REQUIRE(Throws([] { _Znwm_nid_postfix(8); }));
    REQUIRE(handlerCalls == 3 && allocations == 4 && _ZSt15get_new_handlerv_nid_postfix() == nullptr);

    REQUIRE(_ZSt15set_new_handlerPFvvE_nid_postfix(&throwingHandler) == nullptr);
    Reset(1);
    REQUIRE(Throws([] { _Znwm_nid_postfix(8); }) && handlerCalls == 1 && allocations == 1);
    Reset(1);
    REQUIRE(_ZnwmRKSt9nothrow_t_nid_postfix(8, nullptr) == nullptr && handlerCalls == 1 && allocations == 1);
    Reset(1);
    REQUIRE(_ZnamRKSt9nothrow_t_nid_postfix(8, nullptr) == nullptr && handlerCalls == 1 && allocations == 1);

    REQUIRE(_ZSt15set_new_handlerPFvvE_nid_postfix(&allocatingHandler) == &throwingHandler);
    Reset(1);
    REQUIRE(_Znwm_nid_postfix(16) == storage.data() && handlerCalls == 1);

    REQUIRE(_ZSt15set_new_handlerPFvvE_nid_postfix(nullptr) == &allocatingHandler);
    REQUIRE(_ZSt15get_new_handlerv_nid_postfix() == nullptr);

    Reset(0);
    void* aligned = _ZnwmSt11align_val_t_nid_postfix(24, 16);
    REQUIRE(aligned == storage.data() && lastSize == 24 && lastAlignment == 16 && allocations == 1);
    _ZdlPvSt11align_val_t_nid_postfix(aligned, 16);

    Reset(1);
    REQUIRE(Throws([] { _ZnwmSt11align_val_t_nid_postfix(8, 32); }) && handlerCalls == 0 && allocations == 1);

    Reset(0);
    REQUIRE(_ZnwmSt11align_val_t_nid_postfix(0, 16) == storage.data() && lastSize == 1 && lastAlignment == 16);
    Reset(2);
    _ZSt15set_new_handlerPFvvE_nid_postfix(&countingHandler);

    REQUIRE(_ZnwmSt11align_val_t_nid_postfix(8, 16) == storage.data() && handlerCalls == 2 && allocations == 3);
    _ZSt15set_new_handlerPFvvE_nid_postfix(nullptr);

    std::puts("Guest new handler checks passed");
    return 0;
}
