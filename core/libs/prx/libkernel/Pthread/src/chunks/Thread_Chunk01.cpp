extern "C" {
#include "prx/libc/include/GuestGs.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "../include/Pthread.hpp"
#include "../include/ThreadLifecycle.hpp"
#include "../include/WindowsThreadLocal.hpp"
#include "../include/Cancel.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <utility>
#include <vector>
#include <string>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <system_error>

#ifndef _WIN32
#include <alloca.h>
#include <cerrno>
#include <csetjmp>
#include <csignal>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/mach.h>
#endif
#endif

#ifdef __linux__
#include <sys/syscall.h>
#else
static const auto initialThreadId = std::this_thread::get_id();
#endif

extern "C" bool IsInitialPthread_nid_no_patch() {
#ifdef __linux__
    return ::syscall(SYS_gettid) == ::getpid();
#else
    return std::this_thread::get_id() == initialThreadId;
#endif
}

static constexpr int SCE_OK = 0;
static constexpr std::size_t GUEST_THREAD_NAME_CAPACITY = 32;
static constexpr int GUEST_EINVAL = 22;

static constexpr std::size_t HOST_STACK_MARGIN = 1u << 20;
static constexpr int DETACH_DETACHED = 1;
static constexpr std::size_t THREAD_NAME_CAPACITY = 32;
static constexpr std::uint16_t GUEST_THREAD_FPU_CONTROL = 0x037f;

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <immintrin.h>
#endif

struct ThreadArgs {
    PthreadEntry entry;
    void* arg;
    PthreadPrivate* self;
#if defined(_WIN32) || defined(__APPLE__)
    std::uint64_t gsBase = 0;
#endif
#ifndef _WIN32
    sigset_t signalMask;
#endif
    GuestSignalSet guestSignalMask{};
};

#ifdef __APPLE__
static void KeepHostFaultSignalsUnblocked(sigset_t& mask) {
    for (const int signal : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP}) sigdelset(&mask, signal);
}
#endif

static std::atomic<thread_dtors_func_t> threadDtors{nullptr};
static std::atomic<get_thread_atexit_count_func_t> threadAtexitCount{nullptr};
static std::atomic<thread_atexit_report_func_t> threadAtexitReport{nullptr};

struct GuestThreadState {
    PthreadPrivate* current = nullptr;
    bool finishing = false;
    bool exiting = false;
    int cancelState = GUEST_CANCEL_ENABLE;
    int cancelType = GUEST_CANCEL_DEFERRED;
    CleanupHandler* cleanupHandlers = nullptr;
};
static GuestThreadState& CurrentGuestState() {
#ifdef _WIN32
    return WindowsThreadLocal<GuestThreadState>::Get();
#else
    static thread_local GuestThreadState state;
    return state;
#endif
}
static std::mutex& stackLock = *new std::mutex();
static auto& liveStacks = *new std::map<PthreadPrivate*, std::pair<std::uintptr_t, std::uintptr_t>>();
static auto& protectedStackPages = *new std::map<std::uintptr_t, std::size_t>();
#if defined(__linux__)
struct GuestStackMapping {
    std::uintptr_t mappingEnd;
    std::uintptr_t stackStart;
    std::uintptr_t stackEnd;
};
static std::mutex guestStackMappingsLock;
static auto& guestStackMappings = *new std::map<std::uintptr_t, GuestStackMapping>();

static void RegisterGuestStackMapping(PthreadPrivate* thread) {
    const auto mappingStart = reinterpret_cast<std::uintptr_t>(thread->guestStack);
    const auto stackStart = reinterpret_cast<std::uintptr_t>(thread->stackAddress);
    std::lock_guard lock(guestStackMappingsLock);
    guestStackMappings[mappingStart] = {mappingStart + thread->guestStackBytes, stackStart, stackStart + thread->stackSize};
}

static void UnregisterGuestStackMapping(void* mapping) {
    std::lock_guard lock(guestStackMappingsLock);
    guestStackMappings.erase(reinterpret_cast<std::uintptr_t>(mapping));
}

static constexpr std::uintptr_t GUEST_MAIN_STACK_TOP = 0x7eeffc000;
static constexpr int GUEST_MAP_FIXED = 0x10;
static std::atomic<std::uintptr_t> guestMainStackLow{0};
static std::atomic<std::uintptr_t> guestMainStackHigh{0};
#endif

#if defined(__linux__)
static constexpr std::size_t GUEST_STACK_GUARD = PS5_PAGE_SIZE;

struct RetiredStack {
    void* address;
    std::size_t bytes;
    int hostTid;
};

struct FinishedStack {
    void* address;
    std::size_t bytes;
};

static std::mutex retiredStacksLock;
static std::vector<RetiredStack> retiredStacks;
static std::vector<FinishedStack> spareStacks;
static std::size_t spareStackBytes = 0;

static std::size_t SpareStackLimit() {
    static const auto limit = [] {
        const auto pages = sysconf(_SC_PHYS_PAGES);
        const auto pageSize = sysconf(_SC_PAGESIZE);
        if (pages <= 0 || pageSize <= 0) return std::size_t{4u << 20};
        const auto total = static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(pageSize);
        return std::clamp<std::size_t>(static_cast<std::size_t>(total / 1024), 4u << 20, 64u << 20);
    }();
    return limit;
}

static void ReleaseFinishedStack(const FinishedStack& stack) {
    UnregisterGuestStackMapping(stack.address);
    DoMunmap(stack.address, stack.bytes);
}

static bool DiscardFinishedStackPages(const FinishedStack& stack) {
    auto* usable = static_cast<std::byte*>(stack.address) + GUEST_STACK_GUARD;
    const auto bytes = stack.bytes - GUEST_STACK_GUARD;
    return madvise(usable, bytes, MADV_DONTNEED) == 0;
}

static void CollectFinishedStack(void* address, std::size_t bytes) {
    UnregisterGuestStackMapping(address);
    const FinishedStack finished{address, bytes};
    if (!DiscardFinishedStackPages(finished)) {
        ReleaseFinishedStack(finished);
        return;
    }
    std::vector<FinishedStack> release;
    {
        std::lock_guard lock(retiredStacksLock);
        spareStacks.push_back({address, bytes});
        spareStackBytes += bytes;
        const auto limit = SpareStackLimit();
        while (spareStackBytes > limit && !spareStacks.empty()) {
            spareStackBytes -= spareStacks.front().bytes;
            release.push_back(spareStacks.front());
            spareStacks.erase(spareStacks.begin());
        }
    }
    if (!release.empty()) {
        static std::atomic_flag logged = ATOMIC_FLAG_INIT;
        if (!logged.test_and_set(std::memory_order_relaxed))
            std::fprintf(stderr, "scePthread: spare stack cache reached its host-memory limit; releasing cached mappings\n");
        for (const auto& stack : release) ReleaseFinishedStack(stack);
    }
}

static bool TakeSpareStack(std::size_t bytes, FinishedStack* stack) {
    std::lock_guard lock(retiredStacksLock);
    const auto found = std::find_if(spareStacks.begin(), spareStacks.end(), [bytes](const FinishedStack& candidate) { return candidate.bytes == bytes; });
    if (found == spareStacks.end()) return false;
    *stack = *found;
    spareStackBytes -= found->bytes;
    spareStacks.erase(found);
    return true;
}

static void ReleaseSpareStacks() {
    std::vector<FinishedStack> release;
    {
        std::lock_guard lock(retiredStacksLock);
        release.swap(spareStacks);
        spareStackBytes = 0;
    }
    for (const auto& stack : release) ReleaseFinishedStack(stack);
}

static std::size_t GuestMainStackBytes() {
    constexpr std::size_t fallback = std::size_t{8} << 20;
    constexpr std::size_t largest = std::size_t{1} << 30;
    rlimit stack{};
    if (getrlimit(RLIMIT_STACK, &stack) != 0 || stack.rlim_cur == RLIM_INFINITY || stack.rlim_cur == 0) return fallback;
    const auto bytes = static_cast<std::size_t>(std::min<rlim_t>(stack.rlim_cur, largest));
    return (bytes + PS5_PAGE_SIZE - 1) / PS5_PAGE_SIZE * PS5_PAGE_SIZE;
}

extern "C" [[noreturn]] void APS5_VABI Aps5CallOnGuestMainStack_nid_no_patch(std::uintptr_t top, void* arguments, void (APS5_VABI* entry)(void*));

asm(".text\n"
    APS5_ASM_FUNCTION("Aps5CallOnGuestMainStack_nid_no_patch") R"(
    .cfi_startproc
    .cfi_undefined %rip
    mov %rdi, %rsp
    push $0
    push $0
    mov %rsp, %rbp
    mov %rsi, %rdi
    xor %esi, %esi
    call *%rdx
    ud2
    .cfi_endproc
)" APS5_ASM_FUNCTION_END("Aps5CallOnGuestMainStack_nid_no_patch"));

extern "C" [[noreturn]] void APS5_VABI EnterGuestMainStack_nid_no_patch(void* arguments, void (APS5_VABI* entry)(void*)) {
    if (entry == nullptr) throw std::invalid_argument("EnterGuestMainStack: null entry point");
    if (guestMainStackLow.load(std::memory_order_acquire) != 0) throw std::runtime_error("EnterGuestMainStack: the guest main stack was already entered");
    std::uintptr_t arenaBase = 0;
    std::size_t arenaBytes = 0;
    GuestArena::GuestArenaRange_nid_postfix(&arenaBase, &arenaBytes);
    const std::uintptr_t top = GuestArena::GuestAddressOffset + GUEST_MAIN_STACK_TOP;
    const std::size_t bytes = GuestMainStackBytes() + GUEST_STACK_GUARD;
    if (top - bytes < arenaBase || top > arenaBase + arenaBytes)
        throw std::runtime_error("EnterGuestMainStack: the guest arena does not cover the console's main stack range");
    void* mapping = reinterpret_cast<void*>(top - bytes);
    const int mapped = DoMapAnon(&mapping, bytes, 3, GUEST_MAP_FIXED, PS5_PAGE_SIZE, true);
    if (mapped == SCE_KERNEL_ERROR_ENOMEM) {
        char message[160];
        std::snprintf(message, sizeof(message), "EnterGuestMainStack: the guest main stack [%#zx, %#zx) overlaps a guest mapping", static_cast<std::size_t>(top - bytes), static_cast<std::size_t>(top));
        throw std::runtime_error(message);
    }
    if (mapped != 0 || reinterpret_cast<std::uintptr_t>(mapping) + bytes != top)
        throw std::runtime_error("EnterGuestMainStack: cannot map the guest main stack");
    if (DoMprotect(mapping, GUEST_STACK_GUARD, 0) != 0) throw std::runtime_error("EnterGuestMainStack: cannot protect the guest main stack guard");
    guestMainStackHigh.store(top, std::memory_order_release);
    guestMainStackLow.store(reinterpret_cast<std::uintptr_t>(mapping) + GUEST_STACK_GUARD, std::memory_order_release);
    if (auto* self = CurrentGuestState().current) {
        const auto live = LockLiveGuestThread(self);
        if (!live) throw std::runtime_error("EnterGuestMainStack: the current guest thread is not live");
        self->stackAddress = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(mapping) + GUEST_STACK_GUARD);
        self->stackSize = bytes - GUEST_STACK_GUARD;
    }
    Aps5CallOnGuestMainStack_nid_no_patch(top, arguments, entry);
}

static void AllocateGuestStack(PthreadPrivate* thread, std::size_t usableBytes) {
    const std::size_t bytes = (usableBytes + PS5_PAGE_SIZE - 1) / PS5_PAGE_SIZE * PS5_PAGE_SIZE + GUEST_STACK_GUARD;
    FinishedStack spare{};
    if (TakeSpareStack(bytes, &spare)) {
        if (DoMprotect(spare.address, spare.bytes, 3) == 0 && DoMprotect(spare.address, GUEST_STACK_GUARD, 0) == 0) {
            thread->guestStack = spare.address;
            thread->guestStackBytes = spare.bytes;
            return;
        }
        UnregisterGuestStackMapping(spare.address);
        DoMunmap(spare.address, spare.bytes);
    }
    void* stack = nullptr;
    if (DoMapAnon(&stack, bytes, 3, 0) != 0) {
        ReleaseSpareStacks();
        if (DoMapAnon(&stack, bytes, 3, 0) != 0)
            throw std::runtime_error("scePthreadCreate: cannot map the guest thread stack");
    }
    if (DoMprotect(stack, GUEST_STACK_GUARD, 0) != 0) {
        DoMunmap(stack, bytes);
        throw std::runtime_error("scePthreadCreate: cannot protect the guest thread stack guard");
    }
    thread->guestStack = stack;
    thread->guestStackBytes = bytes;
}

static void FreeGuestStack(PthreadPrivate* thread) {
    if (thread->guestStack == nullptr) return;
    CollectFinishedStack(thread->guestStack, thread->guestStackBytes);
    thread->guestStack = nullptr;
    thread->guestStackBytes = 0;
}

static bool HostThreadGone(int hostTid) {
    if (hostTid == 0) return false;
    char path[48];
    std::snprintf(path, sizeof(path), "/proc/self/task/%d", hostTid);
    return access(path, F_OK) != 0;
}

static void ReapRetiredStacks() {
    std::vector<RetiredStack> ready;
    {
        std::lock_guard lock(retiredStacksLock);
        for (auto it = retiredStacks.begin(); it != retiredStacks.end();) {
            if (HostThreadGone(it->hostTid)) {
                ready.push_back(*it);
                it = retiredStacks.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (const auto& stack : ready) CollectFinishedStack(stack.address, stack.bytes);
}

static void RetireGuestStack(PthreadPrivate* thread) {
    if (thread->guestStack == nullptr) return;
    {
        std::lock_guard lock(retiredStacksLock);
        retiredStacks.push_back({thread->guestStack, thread->guestStackBytes, thread->hostTid});
    }
    thread->guestStack = nullptr;
    thread->guestStackBytes = 0;
}
#else
static void FreeGuestStack(PthreadPrivate*) {}
static void RetireGuestStack(PthreadPrivate*) {}
#endif

#ifdef _WIN32
static bool CommittedReadWrite(std::uintptr_t begin, std::uintptr_t end) {
    for (auto cursor = begin; cursor < end;) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) != sizeof(memory) || memory.RegionSize == 0)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Querying the host thread stack");
        if (memory.State != MEM_COMMIT || memory.Protect != PAGE_READWRITE) return false;
        cursor = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    }
    return true;
}

static std::uintptr_t PrecommittedStackStart(std::uintptr_t low, std::uintptr_t high) {
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const std::uintptr_t start = low + system.dwAllocationGranularity;
    return start < high && CommittedReadWrite(start, high) ? start : low;
}
#endif

static bool HostStackLimits(std::uintptr_t* low, std::uintptr_t* high) {
#if defined(__linux__)
    const auto mainLow = guestMainStackLow.load(std::memory_order_acquire);
    const auto mainHigh = guestMainStackHigh.load(std::memory_order_acquire);
    const auto frame = reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
    if (mainLow != 0 && frame >= mainLow && frame < mainHigh) {
        *low = mainLow;
        *high = mainHigh;
        return true;
    }
#endif
#ifdef _WIN32
    ULONG_PTR lowLimit = 0;
    ULONG_PTR highLimit = 0;
    GetCurrentThreadStackLimits(&lowLimit, &highLimit);
    *low = PrecommittedStackStart(lowLimit, highLimit);
    *high = highLimit;
#elif defined(__APPLE__)
    const pthread_t self = pthread_self();
    auto* top = static_cast<std::byte*>(pthread_get_stackaddr_np(self));
    const std::size_t size = pthread_get_stacksize_np(self);
    if (top == nullptr || size == 0) return false;
    *low = reinterpret_cast<std::uintptr_t>(top - size);
    *high = reinterpret_cast<std::uintptr_t>(top);
#else
    static thread_local std::uintptr_t stackLow = 0;
    static thread_local std::uintptr_t stackHigh = 0;
    if (stackHigh == 0) {
        pthread_attr_t attr;
        if (pthread_getattr_np(pthread_self(), &attr) != 0)
            return false;
        void* address = nullptr;
        std::size_t size = 0;
        const bool queried = pthread_attr_getstack(&attr, &address, &size) == 0;
        pthread_attr_destroy(&attr);
        if (!queried || size == 0)
            return false;
        stackLow = reinterpret_cast<std::uintptr_t>(address);
        stackHigh = stackLow + size;
    }
    *low = stackLow;
    *high = stackHigh;
#endif
    return *high > *low;
}

static void SetStackFromHost(PthreadPrivate* thread) {
    std::uintptr_t low = 0;
    std::uintptr_t high = 0;
    if (!HostStackLimits(&low, &high)) throw std::runtime_error("Cannot query the host thread stack");
    const auto start = (low + PS5_PAGE_SIZE - 1) & ~std::uintptr_t{PS5_PAGE_SIZE - 1};
    if (start >= high) throw std::runtime_error("The host thread stack holds no whole guest page");
    thread->stackAddress = reinterpret_cast<void*>(start);
    thread->stackSize = static_cast<std::size_t>(high - start);
}

static bool CurrentStack(std::uintptr_t* low, std::uintptr_t* high) {
    if (CurrentGuestState().current && CurrentGuestState().current->stackAddress) {
        *low = reinterpret_cast<std::uintptr_t>(CurrentGuestState().current->stackAddress);
        *high = *low + CurrentGuestState().current->stackSize;
        return true;
    }
    return HostStackLimits(low, high);
}

static void RegisterStack(PthreadPrivate* self) {
    std::uintptr_t low = 0;
    std::uintptr_t high = 0;
    if (!CurrentStack(&low, &high))
        throw std::runtime_error("Cannot query guest thread stack");
    std::lock_guard lock(stackLock);
    liveStacks[self] = {low, high};
}

static void RestoreStackPages(std::uintptr_t start, std::size_t bytes) noexcept {
#ifdef _WIN32
    DWORD previous = 0;
    const bool restored = VirtualAlloc(reinterpret_cast<void*>(start), bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr &&
        VirtualProtect(reinterpret_cast<void*>(start), bytes, PAGE_READWRITE, &previous) != 0;
#else
    const bool restored = ::mprotect(reinterpret_cast<void*>(start), bytes, PROT_READ | PROT_WRITE) == 0;
#endif
    if (!restored) std::fprintf(stderr, "[thread] cannot restore the protection of guest stack pages 0x%llx+0x%zx at thread exit\n", static_cast<unsigned long long>(start), bytes);
    GuestAllocations::GuestAllocationsInvalidate_nid_postfix(start, bytes);
}

static void ReplaceStackProtection(std::map<std::uintptr_t, std::size_t>& ranges, std::uintptr_t start, std::size_t bytes, bool readWrite) {
    const auto end = start + bytes;
    auto it = ranges.upper_bound(start);
    if (it != ranges.begin() && std::prev(it)->first + std::prev(it)->second > start) --it;
    while (it != ranges.end() && it->first < end) {
        const auto previousStart = it->first;
        const auto previousEnd = previousStart + it->second;
        it = ranges.erase(it);
        if (previousStart < start) ranges.emplace(previousStart, start - previousStart);
        if (previousEnd > end) {
            ranges.emplace(end, previousEnd - end);
            break;
        }
    }
    if (!readWrite) ranges[start] = bytes;
}

void NoteGuestStackProtection(std::uintptr_t start, std::size_t bytes, bool readWrite) {
    std::lock_guard lock(stackLock);
    auto replacement = protectedStackPages;
    ReplaceStackProtection(replacement, start, bytes, readWrite);
    protectedStackPages.swap(replacement);
}

#ifdef _WIN32
bool ReplaceGuestThreadStack(std::uintptr_t start, std::size_t bytes, bool readWrite, const std::function<bool()>& replace) {
    if (bytes == 0 || bytes > std::numeric_limits<std::uintptr_t>::max() - start) return false;
    std::uintptr_t low = 0;
    std::uintptr_t high = 0;
    const bool current = CurrentStack(&low, &high) && start >= low && start + bytes <= high;
    constexpr std::uintptr_t HostFrameReserve = 64 * 1024;
    const auto frame = reinterpret_cast<std::uintptr_t>(&low);
    if (current && (frame < HostFrameReserve || start + bytes > frame - HostFrameReserve)) return false;
    std::lock_guard lock(stackLock);
    if (!current && std::none_of(liveStacks.begin(), liveStacks.end(), [&](const auto& entry) {
            return start >= entry.second.first && start + bytes <= entry.second.second;
        })) return false;
    auto replacement = protectedStackPages;
    ReplaceStackProtection(replacement, start, bytes, readWrite);
    auto failure = protectedStackPages;
    ReplaceStackProtection(failure, start, bytes, false);
    try {
        if (!replace()) return false;
    } catch (...) {
        protectedStackPages.swap(failure);
        throw;
    }
    protectedStackPages.swap(replacement);
    return true;
}
#endif

static void UnregisterStack(PthreadPrivate* self) {
    std::lock_guard lock(stackLock);
    if (const auto found = liveStacks.find(self); found != liveStacks.end()) {
        const auto [low, high] = found->second;
        for (auto it = protectedStackPages.begin(); it != protectedStackPages.end();) {
            if (it->first < high && it->first + it->second > low) {
                RestoreStackPages(it->first, it->second);
                it = protectedStackPages.erase(it);
            } else {
                ++it;
            }
        }
    }
    liveStacks.erase(self);
}

bool GuestThreadStack(std::uintptr_t address, std::uintptr_t* start, std::uintptr_t* end) {
    std::uintptr_t low = 0;
    std::uintptr_t high = 0;
    if (CurrentStack(&low, &high) && address >= low && address < high) {
        *start = low;
        *end = high;
        return true;
    }
    std::lock_guard lock(stackLock);
    for (const auto& [thread, range] : liveStacks) {
        if (address >= range.first && address < range.second) {
            *start = range.first;
            *end = range.second;
            return true;
        }
    }
    return false;
}

}
