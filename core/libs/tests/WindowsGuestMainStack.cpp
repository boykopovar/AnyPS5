#define _WIN32_WINNT 0x0602
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestStacks.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <windows.h>

static DWORD originalThread;
static thread_local std::uint64_t tlsValue;
static std::uint64_t* originalTls;
static std::size_t originalCapacity;

static void Require(bool value) {
    if (!value) std::abort();
}

static void OnGuestStack() {
    std::uintptr_t local = 0;
    Require((reinterpret_cast<std::uintptr_t>(&local) & 0xffffffffc0000000ull) != 0);
    Require(GuestArena::GuestArenaContains_nid_postfix(&local, sizeof(local)));
    Require(GetCurrentThreadId() == originalThread && &tlsValue == originalTls && tlsValue == 0x123456789abcdef0ull);
    ULONG_PTR low = 0;
    ULONG_PTR high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    Require(low <= reinterpret_cast<std::uintptr_t>(&local) && reinterpret_cast<std::uintptr_t>(&local) < high);
    auto* tib = reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    const auto writableLow = reinterpret_cast<std::uintptr_t>(tib->StackLimit);
    Require(high == reinterpret_cast<std::uintptr_t>(tib->StackBase));
    Require(low < writableLow && writableLow <= reinterpret_cast<std::uintptr_t>(&local));
    Require(high - writableLow >= originalCapacity);
    MEMORY_BASIC_INFORMATION guard{};
    Require(VirtualQuery(reinterpret_cast<void*>(writableLow - 1), &guard, sizeof(guard)) == sizeof(guard));
    Require(guard.State != MEM_COMMIT || guard.Protect == PAGE_NOACCESS);
    volatile unsigned char* bottom = reinterpret_cast<volatile unsigned char*>(writableLow);
    bottom[0] = 17;
    bottom[originalCapacity / 2] = 29;
    Require(bottom[0] == 17 && bottom[originalCapacity / 2] == 29);
    bool destroyed = false;
    struct Cleanup {
        bool& destroyed;
        ~Cleanup() { destroyed = true; }
    };
    try {
        Cleanup cleanup{destroyed};
        throw std::runtime_error("guest-stack unwind");
    } catch (const std::runtime_error&) {
        Require(destroyed);
    }
    try {
        GuestStacks::EnterGuestMainStack_nid_no_patch(OnGuestStack);
        std::abort();
    } catch (const std::runtime_error&) {
    }
    void* frames[16]{};
    Require(CaptureStackBackTrace(0, 16, frames, nullptr) != 0);
    std::puts("Guest main stack: placement, TLS, bounds, guard, unwind and duplicate-entry checks passed");
    std::exit(0);
}

int main(int argc, char**) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    originalThread = GetCurrentThreadId();
    originalTls = &tlsValue;
    tlsValue = 0x123456789abcdef0ull;
    auto* tib = reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    const auto base = reinterpret_cast<std::uintptr_t>(tib->StackBase);
    const auto deallocation = *reinterpret_cast<std::uintptr_t*>(reinterpret_cast<unsigned char*>(tib) + 0x1478);
    originalCapacity = base - deallocation;
    if (argc > 1) OnGuestStack();
    try {
        GuestStacks::EnterGuestMainStack_nid_no_patch(nullptr);
        std::abort();
    } catch (const std::invalid_argument&) {
    }
    Require(reinterpret_cast<std::uintptr_t>(tib->StackBase) == base);
    GuestStacks::EnterGuestMainStack_nid_no_patch(OnGuestStack);
}
