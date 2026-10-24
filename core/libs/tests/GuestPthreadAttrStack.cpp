#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGetstack(const PthreadAttr* attr, void** stackAddress, std::size_t* stackSize);
int APS5_VABI scePthreadAttrGetstackaddr(const PthreadAttr* attr, void** stackAddress);
int APS5_VABI scePthreadAttrGetguardsize(const PthreadAttr* attr, std::size_t* guardSize);
int APS5_VABI scePthreadAttrSetguardsize(PthreadAttr* attr, std::size_t guardSize);
int APS5_VABI scePthreadAttrSetstack(PthreadAttr* attr, void* stackAddress, std::size_t stackSize);
int APS5_VABI scePthreadAttrSetstackaddr(PthreadAttr* attr, void* stackAddress);
}

static constexpr int SCE_OK = 0;
static constexpr int SceEinval = static_cast<int>(0x80020016u);
static constexpr std::size_t DefaultStackSize = 1u << 20;
static constexpr std::size_t MinimumStackSize = 16384;

static void Require(bool value) { if (!value) std::abort(); }

alignas(16) static unsigned char stack[65536];

static void RequireStack(const PthreadAttr* attr, const void* expectedAddress, std::size_t expectedSize) {
    void* address = &stack[1];
    std::size_t size = 1;
    Require(scePthreadAttrGetstack(attr, &address, &size) == SCE_OK);
    Require(address == expectedAddress && size == expectedSize);
}

int main() {
    PthreadAttr attr = nullptr;
    PthreadAttr empty = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    void* address = nullptr;
    std::size_t size = 0;

    Require(scePthreadAttrSetstack(nullptr, stack, sizeof(stack)) == SceEinval);
    Require(scePthreadAttrSetstack(&empty, stack, sizeof(stack)) == SceEinval);
    Require(scePthreadAttrSetstack(&attr, nullptr, sizeof(stack)) == SceEinval);
    Require(scePthreadAttrSetstack(&attr, stack, MinimumStackSize - 1) == SceEinval);
    RequireStack(&attr, nullptr, DefaultStackSize);
    Require(scePthreadAttrSetstack(&attr, stack, MinimumStackSize) == SCE_OK);
    RequireStack(&attr, stack, MinimumStackSize);

    Require(scePthreadAttrGetstack(nullptr, &address, &size) == SceEinval);
    Require(scePthreadAttrGetstack(&empty, &address, &size) == SceEinval);
    Require(scePthreadAttrGetstack(&attr, nullptr, &size) == SceEinval);
    Require(scePthreadAttrGetstack(&attr, &address, nullptr) == SceEinval);

    Require(scePthreadAttrSetstackaddr(nullptr, &stack[16]) == SceEinval);
    Require(scePthreadAttrSetstackaddr(&empty, &stack[16]) == SceEinval);
    Require(scePthreadAttrSetstackaddr(&attr, nullptr) == SceEinval);
    Require(scePthreadAttrGetstackaddr(nullptr, &address) == SceEinval);
    Require(scePthreadAttrGetstackaddr(&empty, &address) == SceEinval);
    Require(scePthreadAttrGetstackaddr(&attr, nullptr) == SceEinval);
    Require(scePthreadAttrGetstackaddr(&attr, &address) == SCE_OK && address == stack);
    Require(scePthreadAttrSetstackaddr(&attr, &stack[16]) == SCE_OK);
    Require(scePthreadAttrGetstackaddr(&attr, &address) == SCE_OK && address == &stack[16]);

    std::size_t guard = 0;
    Require(scePthreadAttrSetguardsize(nullptr, 8192) == SceEinval);
    Require(scePthreadAttrSetguardsize(&empty, 8192) == SceEinval);
    Require(scePthreadAttrSetguardsize(&attr, 8192) == SCE_OK);
    Require(scePthreadAttrGetguardsize(nullptr, &guard) == SceEinval);
    Require(scePthreadAttrGetguardsize(&empty, &guard) == SceEinval);
    Require(scePthreadAttrGetguardsize(&attr, nullptr) == SceEinval);
    Require(scePthreadAttrGetguardsize(&attr, &guard) == SCE_OK && guard == 8192);

    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
}
