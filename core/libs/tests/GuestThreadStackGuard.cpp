#include "prx/libkernel/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

static constexpr int SCE_OK = 0;
static constexpr std::uintptr_t GUEST_PAGE = 0x4000;
static constexpr std::size_t GUARD_SIZE = 3 * GUEST_PAGE;
static constexpr std::size_t STACK_SIZES[] = {65536, 1u << 20, 16u << 20};

static void Require(bool value) { if (!value) std::abort(); }

static void* APS5_VABI GuardStackBottom(void* requested) {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGet(scePthreadSelf(), &attr) == SCE_OK);
    void* address = nullptr;
    std::size_t size = 0;
    Require(scePthreadAttrGetstack(&attr, &address, &size) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    Require(start % GUEST_PAGE == 0);
    if (requested != nullptr) Require(size == *static_cast<const std::size_t*>(requested));
    int local = 0;
    Require(reinterpret_cast<std::uintptr_t>(&local) > start + GUARD_SIZE && reinterpret_cast<std::uintptr_t>(&local) < start + size);
    void* low = nullptr;
    void* high = nullptr;
    Require(sceKernelIsStack(&local, &low, &high) == SCE_OK);
    Require(low == address && reinterpret_cast<std::uintptr_t>(high) == start + size);
    for (const int protection : {3, 0, 3}) Require(mprotect_nid_postfix(address, GUARD_SIZE, protection) == 0);
    return nullptr;
}

int main() {
    GuardStackBottom(nullptr);
    for (auto stackSize : STACK_SIZES) {
        PthreadAttr attr = nullptr;
        Require(scePthreadAttrInit(&attr) == SCE_OK);
        Require(scePthreadAttrSetstacksize(&attr, stackSize) == SCE_OK);
        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, &attr, GuardStackBottom, &stackSize, nullptr) == SCE_OK);
        Require(scePthreadAttrDestroy(&attr) == SCE_OK);
        Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    }
}
