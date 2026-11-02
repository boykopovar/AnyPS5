#ifdef __APPLE__

#include "prx/libc/include/GuestArena.hpp"
#include <CoreFoundation/CoreFoundation.h>
#include <crt_externs.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct GuestStart {
    void (*entry)(void*, void*);
    void* block;
};

struct HostStack {
    std::uintptr_t low = 0;
    std::uintptr_t high = 0;
};

constexpr std::uintptr_t MovedStackHint = 0x600000000000ull;
constexpr std::size_t MovedStackBytes = std::size_t {16} << 20;

std::atomic<bool> mainLoopRunning {false};
GuestStart guestStart;
HostStack oldStack;
std::size_t guestStackBytes = 0;

void* RunGuest(void* argument) {
    const auto start = *static_cast<const GuestStart*>(argument);
    start.entry(start.block, nullptr);
    std::fprintf(stderr, "guest entry returned\n");
    std::abort();
}

void KeepAlive(CFRunLoopTimerRef, void*) {}

[[noreturn]] void ServeMainQueue(void*) {
    if (oldStack.high != oldStack.low) {
        if (mach_vm_deallocate(mach_task_self(), oldStack.low, oldStack.high - oldStack.low) != KERN_SUCCESS) {
            std::fprintf(stderr, "cannot release the host stack\n");
            std::abort();
        }
        GuestArena::GuestArenaReleaseHostRegion_nid_no_patch(reinterpret_cast<void*>(oldStack.low), oldStack.high - oldStack.low);
    }
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) std::abort();
    if (pthread_attr_setstacksize(&attributes, guestStackBytes) != 0) std::abort();
    mainLoopRunning.store(true, std::memory_order_release);
    pthread_t thread;
    if (pthread_create(&thread, &attributes, RunGuest, &guestStart) != 0) {
        std::fprintf(stderr, "cannot start the guest thread\n");
        std::abort();
    }
    pthread_attr_destroy(&attributes);
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(nullptr, 1.0e10, 1.0e10, 0, 0, KeepAlive, nullptr);
    CFRunLoopAddTimer(CFRunLoopGetMain(), timer, kCFRunLoopCommonModes);
    for (;;) CFRunLoopRun();
}

#ifdef __x86_64__
bool RegionOf(std::uintptr_t address, std::uintptr_t& begin, std::uintptr_t& end, std::uint32_t& tag) {
    mach_vm_address_t base = address;
    mach_vm_size_t size = 0;
    natural_t depth = 0;
    vm_region_submap_info_data_64_t info {};
    mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
    if (mach_vm_region_recurse(mach_task_self(), &base, &size, &depth, reinterpret_cast<vm_region_recurse_info_t>(&info), &count) != KERN_SUCCESS || base > address) return false;
    begin = base;
    end = base + size;
    tag = info.user_tag;
    return true;
}

HostStack FindHostStack() {
    HostStack stack;
    std::uint32_t tag = 0;
    if (!RegionOf(reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), stack.low, stack.high, tag) || tag != VM_MEMORY_STACK) return {};
    for (;;) {
        std::uintptr_t begin = 0, end = 0;
        if (!RegionOf(stack.low - 1, begin, end, tag) || tag != VM_MEMORY_STACK || end != stack.low) break;
        stack.low = begin;
    }
    const pthread_t self = pthread_self();
    const auto top = reinterpret_cast<std::uintptr_t>(pthread_get_stackaddr_np(self));
    const std::uintptr_t size = pthread_get_stacksize_np(self);
    stack.low = std::max(stack.low, top - std::min(top, size));
    stack.high = std::min(stack.high, top);
    if (stack.low >= stack.high) return {};
    return stack;
}

char** CopyStrings(char** source) {
    std::size_t count = 0;
    while (source[count] != nullptr) ++count;
    auto** copy = static_cast<char**>(std::calloc(count + 1, sizeof(char*)));
    for (std::size_t index = 0; index < count; ++index) copy[index] = strdup(source[index]);
    return copy;
}

void* CopyBlock(void* block) {
    if (block == nullptr) return nullptr;
    const auto* words = static_cast<const std::uint64_t*>(block);
    std::size_t count = 1 + words[0] + 1;
    while (words[count] != 0) ++count;
    ++count;
    auto* copy = static_cast<std::uint64_t*>(std::calloc(count, sizeof(std::uint64_t)));
    std::memcpy(copy, words, count * sizeof(std::uint64_t));
    for (std::size_t index = 1; index < count; ++index) {
        if (copy[index] != 0) copy[index] = reinterpret_cast<std::uint64_t>(strdup(reinterpret_cast<const char*>(copy[index])));
    }
    return copy;
}

[[noreturn]] void RunOnStack(void* top, void (*function)(void*)) {
    asm volatile("mov %0, %%rsp\n\txor %%edi, %%edi\n\tcall *%1\n\tud2" : : "r"(top), "r"(function) : "rdi", "memory");
    __builtin_unreachable();
}

void MoveHostStack() {
    oldStack = FindHostStack();
    if (oldStack.high == oldStack.low || !GuestArena::GuestArenaOverlapsHostRegion_nid_no_patch(oldStack.low, oldStack.high - oldStack.low)) {
        oldStack = {};
        return;
    }
    void* base = mmap(reinterpret_cast<void*>(MovedStackHint), MovedStackBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    if (base == MAP_FAILED) {
        oldStack = {};
        return;
    }
    const auto at = reinterpret_cast<std::uintptr_t>(base);
    if (GuestArena::GuestArenaOverlapsHostRegion_nid_no_patch(at, MovedStackBytes)) {
        munmap(base, MovedStackBytes);
        oldStack = {};
        return;
    }
    guestStart.block = CopyBlock(guestStart.block);
    *_NSGetEnviron() = CopyStrings(*_NSGetEnviron());
    *_NSGetArgv() = CopyStrings(*_NSGetArgv());
    setprogname(strdup(getprogname()));
    mprotect(base, static_cast<std::size_t>(getpagesize()), PROT_NONE);
    RunOnStack(reinterpret_cast<void*>((at + MovedStackBytes) & ~std::uintptr_t {15}), &ServeMainQueue);
}
#else
void MoveHostStack() {}
#endif

}

extern "C" {

[[noreturn]] void Aps5StartGuest_nid_no_patch(void (*entry)(void*, void*), void* block) {
    guestStart = {entry, block};
    guestStackBytes = std::max<std::size_t>(pthread_get_stacksize_np(pthread_self()), 8u << 20);
    MoveHostStack();
    ServeMainQueue(nullptr);
}

void Aps5RunOnMainThread_nid_no_patch(void (*function)(void*), void* context) {
    if (pthread_main_np() != 0 || !mainLoopRunning.load(std::memory_order_acquire)) {
        function(context);
        return;
    }
    dispatch_sync_f(dispatch_get_main_queue(), context, function);
}

}

#endif
