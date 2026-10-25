#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "../include/Pthread.hpp"
#include "../include/ThreadLifecycle.hpp"
#include "../include/WindowsThreadLocal.hpp"
#include "../include/Cancel.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include <algorithm>
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
#include <cerrno>
#include <csetjmp>
#include <csignal>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
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

static constexpr std::size_t HOST_STACK_MARGIN = 1u << 20;
static constexpr int DETACH_DETACHED = 1;
static constexpr std::size_t THREAD_NAME_CAPACITY = 32;

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <immintrin.h>
#endif

struct ThreadArgs {
    PthreadEntry entry;
    void* arg;
    PthreadPrivate* self;
#ifndef _WIN32
    sigset_t signalMask;
#endif
};

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

extern "C" [[noreturn]] void APS5_VABI EnterGuestMainStack_nid_no_patch(void* arguments, void (*entry)(void*)) {
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
    __asm__ volatile(
        "mov %0, %%rsp\n\t"
        "pushq $0\n\t"
        "pushq $0\n\t"
        "mov %%rsp, %%rbp\n\t"
        "xor %%esi, %%esi\n\t"
        "call *%2\n\t"
        "ud2"
        :
        : "r"(top), "D"(arguments), "a"(entry)
        : "memory");
    __builtin_unreachable();
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
    thread->stackAddress = reinterpret_cast<void*>(low);
    thread->stackSize = static_cast<std::size_t>(high - low);
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

bool GuestThreadStackHostRangeOverlaps(std::uintptr_t start, std::uintptr_t end) {
#if defined(__linux__)
    std::lock_guard lock(guestStackMappingsLock);
    auto it = guestStackMappings.upper_bound(start);
    if (it != guestStackMappings.begin()) --it;
    for (; it != guestStackMappings.end() && it->first < end; ++it) {
        const auto mappingStart = it->first;
        const auto& stack = it->second;
        if (start < stack.stackStart && end > mappingStart) return true;
        if (start < stack.mappingEnd && end > stack.stackEnd) return true;
    }
#else
    (void)start;
    (void)end;
#endif
    return false;
}

struct CpuClockRegistry {
    std::mutex lock;
    std::map<int, PthreadPrivate*> threads;
    int nextThread = 100000;
    KernelCpumask processAffinity = DEFAULT_THREAD_AFFINITY;
};

static CpuClockRegistry& CpuClocks() {
    static auto* registry = new CpuClockRegistry();
    return *registry;
}

PthreadPrivate::PthreadPrivate(const PthreadAttrPrivate* attr) : _finished(false), _retval(nullptr), _detached(false), _adopted(false), _handleStorage(nullptr), _joined(false) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    const auto* parent = CurrentGuestState().current;
    const KernelCpumask inherited = parent ? parent->affinity.load(std::memory_order_relaxed) : registry.processAffinity;
    affinity.store(attr && attr->_affinityExplicit ? attr->_affinity : inherited, std::memory_order_relaxed);
    constexpr int ExplicitSched = 0;
    const bool explicitSched = attr && attr->_inheritsched == ExplicitSched;
    const int inheritedPolicy = parent ? parent->schedPolicy.load(std::memory_order_relaxed) : schedPolicy.load(std::memory_order_relaxed);
    schedPolicy.store(explicitSched ? attr->_schedpolicy : inheritedPolicy, std::memory_order_relaxed);
    const int attrPriority = attr ? attr->_schedpriority : DEFAULT_THREAD_PRIORITY;
    priority.store(explicitSched || !parent ? attrPriority : parent->priority.load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (registry.nextThread > static_cast<int>(CPU_CLOCK_ID_MASK))
        throw std::runtime_error("Guest thread CPU clock identifiers are exhausted");
    cpuClockThread = registry.nextThread++;
    registry.threads.emplace(cpuClockThread, this);
}

PthreadPrivate::~PthreadPrivate() {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    registry.threads.erase(cpuClockThread);
}

static void BindHostCpuClock(PthreadPrivate* self) {
#ifdef _WIN32
    (void)self;
#else
    clockid_t clock{};
    const int error = pthread_getcpuclockid(pthread_self(), &clock);
    if (error != 0)
        throw std::system_error(error, std::generic_category(), "Querying the guest thread CPU clock");
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    self->hostCpuClock = clock;
    self->hostCpuClockBound = true;
#endif
}

int GuestThreadCpuClockId(const PthreadPrivate* thread) {
    return static_cast<int>(CPU_CLOCK_BIT | static_cast<std::uint32_t>(thread->cpuClockThread));
}

void SetLiveGuestThreadsAffinity(KernelCpumask mask) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    registry.processAffinity = mask;
    for (const auto& [id, thread] : registry.threads) {
        if (!thread->_finished.load(std::memory_order_acquire)) thread->affinity.store(mask, std::memory_order_relaxed);
    }
}

KernelCpumask LiveGuestThreadsAffinity() {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    KernelCpumask mask = 0;
    for (const auto& [id, thread] : registry.threads) {
        if (!thread->_finished.load(std::memory_order_acquire)) mask |= thread->affinity.load(std::memory_order_relaxed);
    }
    return mask;
}

std::unique_lock<std::mutex> LockLiveGuestThread(const PthreadPrivate* thread) {
    auto& registry = CpuClocks();
    std::unique_lock lock(registry.lock);
    for (const auto& [id, live] : registry.threads) {
        if (live == thread && !live->_finished.load(std::memory_order_acquire)) return lock;
    }
    return {};
}

bool GuestThreadIsLive(const PthreadPrivate* thread) {
    return LockLiveGuestThread(thread).owns_lock();
}

bool GetLiveGuestThreadAffinity_nid_no_patch(std::int64_t tid, KernelCpumask& mask) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    for (const auto& [id, thread] : registry.threads) {
        if (thread->tid != tid || thread->_finished.load(std::memory_order_acquire)) continue;
        mask = thread->affinity.load(std::memory_order_relaxed);
        return true;
    }
    return false;
}

bool GetLiveGuestThreadSched_nid_no_patch(std::int64_t tid, int& policy, int& priority) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    for (const auto& [id, thread] : registry.threads) {
        if (thread->tid != tid || thread->_finished.load(std::memory_order_acquire)) continue;
        policy = thread->schedPolicy.load(std::memory_order_relaxed);
        priority = thread->priority.load(std::memory_order_relaxed);
        return true;
    }
    return false;
}

bool SetLiveGuestThreadAffinity_nid_no_patch(std::int64_t tid, KernelCpumask mask) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    for (const auto& [id, thread] : registry.threads) {
        if (thread->tid != tid || thread->_finished.load(std::memory_order_acquire)) continue;
        thread->affinity.store(mask, std::memory_order_relaxed);
        return true;
    }
    return false;
}

bool LiveGuestThreadExists(std::int64_t tid) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    for (const auto& [id, thread] : registry.threads) {
        if (!thread->_finished.load(std::memory_order_acquire) && thread->tid == tid) return true;
    }
    return false;
}

bool GuestThreadCpuNanos(int cpuClockThread, std::uint64_t* nanos) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    const auto found = registry.threads.find(cpuClockThread);
    if (found == registry.threads.end())
        return false;
    PthreadPrivate* thread = found->second;
    if (thread->_finished.load(std::memory_order_acquire))
        return false;
#ifdef _WIN32
    if (thread->nativeHandle == nullptr) {
        *nanos = 0;
        return true;
    }
    if (WaitForSingleObject(thread->nativeHandle, 0) == WAIT_OBJECT_0)
        return false;
    FILETIME creation{};
    FILETIME exitTime{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetThreadTimes(thread->nativeHandle, &creation, &exitTime, &kernel, &user))
        throw std::system_error(GetLastError(), std::system_category(), "Querying guest thread CPU time");
    const auto toTicks = [](const FILETIME& time) {
        return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    };
    *nanos = (toTicks(user) + toTicks(kernel)) * 100ULL;
#else
    if (!thread->hostCpuClockBound) {
        *nanos = 0;
        return true;
    }
    struct timespec time{};
    if (clock_gettime(thread->hostCpuClock, &time) != 0)
        throw std::system_error(errno, std::generic_category(), "Querying guest thread CPU time");
    *nanos = static_cast<std::uint64_t>(time.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(time.tv_nsec);
#endif
    return true;
}
#ifndef _WIN32
static thread_local std::unique_ptr<PthreadPrivate> adoptedThread;
// pthread_exit force-unwinds through guest frames, whose personality resolves to
// libc.prx's __gxx_personality_v0; that routine cannot read libgcc's unwind context
// and aborts. scePthreadExit instead jumps back to the thread start routine, which,
// like _endthreadex on Windows, skips the guest frames without unwinding them.
static thread_local std::jmp_buf* threadExitJump = nullptr;

static void* MeasureStackReserve(void* reserve) {
    std::uintptr_t low = 0;
    std::uintptr_t high = 0;
    if (HostStackLimits(&low, &high))
        *static_cast<std::size_t*>(reserve) = high - reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
    return nullptr;
}

static std::size_t HostStackReserve() {
    static const std::size_t reserve = [] {
        std::size_t measured = 0;
        pthread_t probe{};
        if (const int result = pthread_create(&probe, nullptr, MeasureStackReserve, &measured); result != 0)
            throw std::system_error(result, std::generic_category(), "Measuring the host thread stack reserve");
        if (const int result = pthread_join(probe, nullptr); result != 0)
            throw std::system_error(result, std::generic_category(), "Measuring the host thread stack reserve");
        if (measured == 0)
            throw std::runtime_error("Cannot measure the host thread stack reserve");
        return measured;
    }();
    return reserve;
}
#endif

void ThreadLifecycle::SetThreadDtors(thread_dtors_func_t callback) {
    if (!callback)
        throw std::runtime_error("Thread destructor callback is null");
    thread_dtors_func_t expected = nullptr;
    if (!threadDtors.compare_exchange_strong(expected, callback))
        throw std::runtime_error("Thread destructor callback is already registered");
}

void ThreadLifecycle::SetThreadAtexitCount(get_thread_atexit_count_func_t callback) {
    if (!callback)
        throw std::runtime_error("Thread atexit count callback is null");
    get_thread_atexit_count_func_t expected = nullptr;
    if (!threadAtexitCount.compare_exchange_strong(expected, callback))
        throw std::runtime_error("Thread atexit count callback is already registered");
}

void ThreadLifecycle::SetThreadAtexitReport(thread_atexit_report_func_t callback) {
    if (!callback)
        throw std::runtime_error("Thread atexit report callback is null");
    thread_atexit_report_func_t expected = nullptr;
    if (!threadAtexitReport.compare_exchange_strong(expected, callback))
        throw std::runtime_error("Thread atexit report callback is already registered");
}

static void finishThread(PthreadPrivate* self, void* retval) {
    if (!self || self != CurrentGuestState().current)
        throw std::runtime_error("Finishing an unregistered guest thread");
    if (CurrentGuestState().finishing)
        throw std::runtime_error("Guest thread is already finishing");
    CurrentGuestState().finishing = true;
    CurrentGuestState().exiting = true;
    UnregisterStack(self);
    if (const auto callback = threadDtors.load())
        callback();
    ThreadLifecycle::RunKeyDtors();
    {
        std::unique_lock<std::mutex> lk(self->_join_mtx);
        self->_retval = retval;
        self->_finished.store(true, std::memory_order_release);
    }
    self->_join_cv.NotifyAll();
}

bool ThreadCancel::Requested() {
    return CurrentGuestState().current && !CurrentGuestState().exiting && CurrentGuestState().cancelState == GUEST_CANCEL_ENABLE && CurrentGuestState().current->cancelPending.load();
}

void ThreadCancel::Check() {
    if (Requested()) scePthreadExit(GUEST_CANCELED);
}

void* ThreadCancel::CleanupMark() {
    return CurrentGuestState().cleanupHandlers;
}

void ThreadCancel::CleanupRestore(void* mark) {
    CurrentGuestState().cleanupHandlers = static_cast<CleanupHandler*>(mark);
}

void ThreadCancel::BeginWait(TimedWait::Condition* condition) {
    if (!CurrentGuestState().current) return;
    std::lock_guard lock(CurrentGuestState().current->cancelLock);
    CurrentGuestState().current->cancelWait = condition;
}

void ThreadCancel::EndWait() {
    BeginWait(nullptr);
}

extern "C" void* APS5_VABI Aps5CallGuestEntry_nid_no_patch(PthreadEntry entry, void* arg);

asm(".text\n"
    APS5_ASM_FUNCTION("Aps5CallGuestEntry_nid_no_patch") R"(
    .cfi_startproc
    push %rbp
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %rbp, 0
    push $0
    .cfi_adjust_cfa_offset 8
    push $0
    .cfi_adjust_cfa_offset 8
    mov %rsp, %rbp
    mov %rdi, %rax
    mov %rsi, %rdi
    call *%rax
    add $16, %rsp
    .cfi_adjust_cfa_offset -16
    pop %rbp
    .cfi_adjust_cfa_offset -8
    .cfi_restore %rbp
    ret
    .cfi_endproc
)" APS5_ASM_FUNCTION_END("Aps5CallGuestEntry_nid_no_patch"));

static void RunThread(std::unique_ptr<ThreadArgs> args) {
    APS5_LOG_OUT("RunThread entry=0x%llx arg=%p self=%p", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(args->entry)), args->arg, static_cast<void*>(args->self));
    const auto entry = args->entry;
    void* arg = args->arg;
    PthreadPrivate* self = args->self;
    BindHostCpuClock(self);
    TimedWait::BindThreadWaitState(&self->waitCount);
    CurrentGuestState().current = self;
    RegisterStack(self);
#ifndef _WIN32
    const sigset_t signalMask = args->signalMask;
#endif
    args.reset();
#ifndef _WIN32
    pthread_sigmask(SIG_SETMASK, &signalMask, nullptr);
#endif
    finishThread(self, Aps5CallGuestEntry_nid_no_patch(entry, arg));
    CurrentGuestState().current = nullptr;
}

static void ReleaseThread(PthreadPrivate* thread) {
    if (thread->_adopted)
        return;
    if (thread->references.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return;
#ifdef _WIN32
    if (thread->wakeEvent)
        CloseHandle(static_cast<HANDLE>(thread->wakeEvent));
    if (!CloseHandle(thread->nativeHandle))
        throw std::system_error(GetLastError(), std::system_category(), "Closing guest thread handle");
#endif
    delete thread;
}

static std::mutex joinedLock;
static std::map<Pthread*, PthreadPrivate*> joinedThreads;

static void RetainJoinedThread(PthreadPrivate* thread) {
    std::lock_guard lock(joinedLock);
    Pthread* const storage = thread->_handleStorage;
    const auto found = joinedThreads.find(storage);
    if (found != joinedThreads.end() && found->second != thread) ReleaseThread(found->second);
    joinedThreads[storage] = thread;
}

static void RecycleJoinedThread(Pthread* storage) {
    std::lock_guard lock(joinedLock);
    const auto found = joinedThreads.find(storage);
    if (found == joinedThreads.end()) return;
    PthreadPrivate* joined = found->second;
    joinedThreads.erase(found);
    ReleaseThread(joined);
}

#ifdef _WIN32
// The title's job workers ("BPE JobWorkerThread CPU0".."CPU12") spin at full load on one core each.
// Pinning them to the efficiency cores of a hybrid CPU (APS5_JOB_AFFINITY=1, with the driver's
// worker pinning) measured 0 to -9 % at the video stage, so the default leaves them free;
// APS5_JOB_AFFINITY_MASK=<hex> picks the cores.
static constexpr const char* JOB_WORKER_PREFIX = "BPE JobWorkerThread";

static std::uint64_t JobWorkerMask() {
    static const std::uint64_t mask = [] {
        if (std::getenv("APS5_NO_JOB_AFFINITY") != nullptr) return std::uint64_t{0};
        const auto requested = CpuTopology::MaskFromEnvironment("APS5_JOB_AFFINITY_MASK");
        if (requested != 0) return requested;
        if (std::getenv("APS5_JOB_AFFINITY") == nullptr) return std::uint64_t{0};
        const auto& layout = CpuTopology::Get();
        return layout.hybrid ? layout.efficient : std::uint64_t{0};
    }();
    return mask;
}

static void ApplyJobAffinity(PthreadPrivate& thread, void* handle) {
    if (thread.name.compare(0, std::strlen(JOB_WORKER_PREFIX), JOB_WORKER_PREFIX) != 0) return;
    const auto mask = JobWorkerMask();
    if (mask == 0) return;
    static std::once_flag summary;
    std::call_once(summary, [mask] {
        const auto& layout = CpuTopology::Get();
        std::fprintf(stderr, "[affinity] job worker threads -> 0x%llx (hybrid=%d efficient=0x%llx performant=0x%llx process=0x%llx)\n", static_cast<unsigned long long>(mask), layout.hybrid ? 1 : 0, static_cast<unsigned long long>(layout.efficient), static_cast<unsigned long long>(layout.performant), static_cast<unsigned long long>(layout.process));
    });
    CpuTopology::PinTraced(thread.name.c_str(), handle, mask);
}
#endif

struct NativeThreadArgs {
    std::unique_ptr<ThreadArgs> guest;
    std::future<bool> start;
    std::promise<void> initialized;
};

#ifdef _WIN32
static unsigned __stdcall StartNativeThread(void* opaque) {
    std::unique_ptr<NativeThreadArgs> args(static_cast<NativeThreadArgs*>(opaque));
    auto* self = args->guest->self;
    try {
        ULONG_PTR low = 0;
        ULONG_PTR high = 0;
        GetCurrentThreadStackLimits(&low, &high);
        if (high <= low || high - low < self->stackSize)
            throw std::runtime_error("Cannot query guest thread stack");
        self->stackAddress = reinterpret_cast<void*>(high - self->stackSize);
        if (!CommittedReadWrite(high - self->stackSize, high))
            throw std::runtime_error("Guest thread stack is not fully committed");
        self->threadId = std::this_thread::get_id();
        CurrentGuestState().current = self;
        if (!self->name.empty()) {
            const std::wstring description(self->name.begin(), self->name.end());
            SetThreadDescription(GetCurrentThread(), description.c_str());
            ApplyJobAffinity(*self, nullptr);
        }
        args->initialized.set_value();
    } catch (...) {
        args->initialized.set_exception(std::current_exception());
        return 0;
    }
    if (!args->start.get())
        return 0;
    auto guest = std::move(args->guest);
    args.reset();
    RunThread(std::move(guest));
    CurrentGuestState().current = nullptr;
    ReleaseThread(self);
    return 0;
}
#else
static void* StartNativeThread(void* opaque) {
    std::unique_ptr<NativeThreadArgs> args(static_cast<NativeThreadArgs*>(opaque));
    auto* self = args->guest->self;
    try {
        std::uintptr_t low = 0;
        std::uintptr_t high = 0;
        const auto reserve = HostStackReserve();
        if (!HostStackLimits(&low, &high) || high - low < reserve + self->stackSize)
            throw std::runtime_error("Cannot query guest thread stack");
        self->stackAddress = reinterpret_cast<void*>(high - reserve - self->stackSize);
        self->threadId = std::this_thread::get_id();
#if defined(__linux__)
        self->hostTid = static_cast<int>(gettid());
        RegisterGuestStackMapping(self);
#endif
        args->initialized.set_value();
    } catch (...) {
        args->initialized.set_exception(std::current_exception());
        return nullptr;
    }
    if (!args->start.get())
        return nullptr;
    auto* guest = args->guest.release();
    args.reset();
    struct ThreadGuard {
        PthreadPrivate* self;
        ~ThreadGuard() {
            CurrentGuestState().current = nullptr;
            ReleaseThread(self);
        }
    } guard{self};
    std::jmp_buf exitJump;
    if (setjmp(exitJump) == 0) {
        threadExitJump = &exitJump;
        RunThread(std::unique_ptr<ThreadArgs>(guest));
    }
    threadExitJump = nullptr;
    return nullptr;
}
#endif

extern "C" {

int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    if (!thread || !entry) throw std::runtime_error("scePthreadCreate: null arg");
    if (name && std::strlen(name) >= GUEST_THREAD_NAME_CAPACITY) return SCE_KERNEL_ERROR_ENAMETOOLONG;
    if (attr && !*attr) attr = nullptr;
    auto p = std::make_unique<PthreadPrivate>(attr ? *attr : nullptr);
    bool detached = false;
    if (attr && *attr) detached = ((*attr)->_detachstate == DETACH_DETACHED);
    p->_detached = detached;
    p->stackSize = attr ? (*attr)->_stacksize : GetDefaultStackSize_nid_no_patch();
    if (attr) {
        if ((*attr)->_inheritsched != GUEST_INHERIT_SCHED)
            p->scope = (*attr)->_scope;
        else if (CurrentGuestState().current)
            p->scope = CurrentGuestState().current->scope;
    } else if (CurrentGuestState().current) {
        p->scope = CurrentGuestState().current->scope;
    }
    if (name) p->name = name;
    std::promise<bool> start;
    auto args = std::make_unique<ThreadArgs>(ThreadArgs{entry, arg, p.get()});
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    if (p->stackSize < 16384 || p->stackSize > std::numeric_limits<unsigned>::max() - HOST_STACK_MARGIN - system.dwPageSize)
        throw std::runtime_error("scePthreadCreate: invalid Windows stack size");
    const std::size_t nativeStack = (p->stackSize + HOST_STACK_MARGIN + system.dwPageSize - 1) / system.dwPageSize * system.dwPageSize;
    auto native = std::make_unique<NativeThreadArgs>(NativeThreadArgs{std::move(args), start.get_future(), {}});
    auto initialized = native->initialized.get_future();
    const auto handle = _beginthreadex(nullptr, static_cast<unsigned>(nativeStack), StartNativeThread, native.get(), 0, nullptr);
    if (handle == 0)
        throw std::system_error(errno, std::generic_category(), "Creating guest thread");
    p->nativeHandle = reinterpret_cast<void*>(handle);
    native.release();
    try {
        initialized.get();
    } catch (...) {
        start.set_value(false);
        WaitForSingleObject(p->nativeHandle, INFINITE);
        CloseHandle(p->nativeHandle);
        throw;
    }
    auto* published = p.release();
    published->_handleStorage = thread;
    *thread = published;
    start.set_value(true);
    RecycleJoinedThread(thread);
    if (detached)
        ReleaseThread(published);
#else
    const auto reserve = HostStackReserve();
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    if (p->stackSize < 16384 || p->stackSize > std::numeric_limits<std::size_t>::max() - reserve - HOST_STACK_MARGIN - page)
        throw std::runtime_error("scePthreadCreate: invalid stack size");
    const std::size_t nativeStack = (p->stackSize + reserve + HOST_STACK_MARGIN + page - 1) / page * page;
    sigset_t creatorMask;
    pthread_sigmask(SIG_SETMASK, nullptr, &creatorMask);
    args->signalMask = creatorMask;
#if defined(__linux__)
    ReapRetiredStacks();
    AllocateGuestStack(p.get(), nativeStack);
#endif
    auto native = std::make_unique<NativeThreadArgs>(NativeThreadArgs{std::move(args), start.get_future(), {}});
    auto initialized = native->initialized.get_future();
    pthread_attr_t nativeAttr;
    if (const int result = pthread_attr_init(&nativeAttr); result != 0) {
        FreeGuestStack(p.get());
        throw std::system_error(result, std::generic_category(), "Creating guest thread");
    }
#if defined(__linux__)
    int created = pthread_attr_setstack(&nativeAttr, static_cast<char*>(p->guestStack) + GUEST_STACK_GUARD, p->guestStackBytes - GUEST_STACK_GUARD);
#else
    int created = pthread_attr_setstacksize(&nativeAttr, nativeStack);
#endif
    if (created == 0) {
        sigset_t allSignals;
        sigfillset(&allSignals);
        pthread_sigmask(SIG_BLOCK, &allSignals, nullptr);
        created = pthread_create(&p->hostThread, &nativeAttr, StartNativeThread, native.get());
        pthread_sigmask(SIG_SETMASK, &creatorMask, nullptr);
    }
    pthread_attr_destroy(&nativeAttr);
    if (created != 0) {
        FreeGuestStack(p.get());
        throw std::system_error(created, std::generic_category(), "Creating guest thread");
    }
    native.release();
    try {
        initialized.get();
        if (detached) {
            if (const int result = pthread_detach(p->hostThread); result != 0)
                throw std::system_error(result, std::generic_category(), "Detaching guest thread");
            RetireGuestStack(p.get());
        }
    } catch (...) {
        start.set_value(false);
        pthread_join(p->hostThread, nullptr);
        FreeGuestStack(p.get());
        throw;
    }
    auto* published = p.release();
    published->_handleStorage = thread;
    *thread = published;
    start.set_value(true);
    RecycleJoinedThread(thread);
    if (detached)
        ReleaseThread(published);
#endif
    return SCE_OK;
}

int APS5_VABI JoinThread_nid_no_patch(Pthread thread, void** retval, const KernelTimespec* abstime) {
    if (!thread) return SCE_KERNEL_ERROR_EINVAL;
    if (thread == CurrentGuestState().current) return SCE_KERNEL_ERROR_EDEADLK;
    if (thread->_detached) return SCE_KERNEL_ERROR_EINVAL;
    if (thread->_joined.load(std::memory_order_acquire)) return SCE_KERNEL_ERROR_ESRCH;
    ThreadCancel::Check();
    {
        std::unique_lock lock(thread->_join_mtx);
        if (thread->_detached) return SCE_KERNEL_ERROR_EINVAL;
        if (thread->_joining) return SCE_KERNEL_ERROR_EOPNOTSUPP;
        thread->_joining = true;
        while (!thread->_finished.load()) {
            std::optional<std::uint64_t> deadline;
            if (abstime != nullptr) {
                KernelTimespec now{};
                if (clock_gettime_nid_postfix(0, &now) != 0) {
                    thread->_joining = false;
                    throw std::runtime_error("Joining guest thread: realtime clock unavailable");
                }
                if (abstime->tv_sec < now.tv_sec || (abstime->tv_sec == now.tv_sec && abstime->tv_nsec <= now.tv_nsec)) {
                    thread->_joining = false;
                    return SCE_KERNEL_ERROR_ETIMEDOUT;
                }
                const auto seconds = static_cast<std::uint64_t>(abstime->tv_sec) - static_cast<std::uint64_t>(now.tv_sec);
                const auto nanos = seconds > 1 ? std::int64_t{1000000000} : static_cast<std::int64_t>(seconds) * 1000000000 + abstime->tv_nsec - now.tv_nsec;
                deadline = TimedWait::DeadlineNanos(static_cast<std::uint64_t>((nanos + 999) / 1000));
            }
            ThreadCancel::Wait(thread->_join_cv, lock, deadline);
            if (ThreadCancel::Requested()) {
                thread->_joining = false;
                lock.unlock();
                ThreadCancel::Check();
                lock.lock();
                thread->_joining = true;
            }
        }
        thread->_joining = false;
        if (thread->_joined.exchange(true, std::memory_order_acq_rel)) return SCE_KERNEL_ERROR_ESRCH;
    }
#ifdef _WIN32
    if (WaitForSingleObject(thread->nativeHandle, INFINITE) != WAIT_OBJECT_0)
        throw std::system_error(GetLastError(), std::system_category(), "Joining guest thread");
    if (retval) *retval = thread->_retval;
#else
    if (const int result = pthread_join(thread->hostThread, nullptr); result != 0)
        throw std::system_error(result, std::generic_category(), "Joining guest thread");
    FreeGuestStack(thread);
    if (retval) *retval = thread->_retval;
#endif
    RetainJoinedThread(thread);
    return SCE_OK;
}

int APS5_VABI scePthreadJoin(Pthread thread, void** retval) {
    return JoinThread_nid_no_patch(thread, retval, nullptr);
}

int APS5_VABI scePthreadDetach(Pthread thread) {
    if (!thread) return SCE_KERNEL_ERROR_EINVAL;
    {
        std::lock_guard lock(thread->_join_mtx);
        if (thread->_joined.load(std::memory_order_acquire)) return SCE_KERNEL_ERROR_ESRCH;
        if (thread->_detached || thread->_joining) return SCE_KERNEL_ERROR_EINVAL;
        thread->_detached = true;
    }
#ifdef _WIN32
    ReleaseThread(thread);
#else
    if (const int result = pthread_detach(thread->hostThread); result != 0)
        throw std::system_error(result, std::generic_category(), "Detaching guest thread");
    RetireGuestStack(thread);
    ReleaseThread(thread);
#endif
    return SCE_OK;
}

void APS5_VABI scePthreadExit(void* retval) {
    if (!CurrentGuestState().current)
        throw std::runtime_error("scePthreadExit: current thread is not registered");
    auto* self = CurrentGuestState().current;
    CurrentGuestState().exiting = true;
    while (CurrentGuestState().cleanupHandlers) __pthread_cleanup_pop_imp_nid_postfix(1);
    finishThread(self, retval);
    CurrentGuestState().current = nullptr;
#ifdef _WIN32
    ReleaseThread(self);
    _endthreadex(0);
#else
    if (threadExitJump)
        std::longjmp(*threadExitJump, 1);
    pthread_exit(retval);
#endif
    throw std::runtime_error("Native thread exit returned");
}

Pthread APS5_VABI scePthreadSelf() {
#ifdef _WIN32
    if (!CurrentGuestState().current) {
        auto adopted = std::make_unique<PthreadPrivate>();
        HANDLE handle = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
            throw std::system_error(GetLastError(), std::system_category(), "Adopting guest thread");
        adopted->nativeHandle = handle;
        adopted->threadId = std::this_thread::get_id();
        adopted->_detached = true;
        adopted->references.store(1, std::memory_order_relaxed);
        TimedWait::BindThreadWaitState(&adopted->waitCount);
        SetStackFromHost(adopted.get());
        CurrentGuestState().current = adopted.release();
    }
#else
    if (!CurrentGuestState().current) {
        adoptedThread = std::make_unique<PthreadPrivate>();
        adoptedThread->_detached = true;
        adoptedThread->_adopted = true;
        BindHostCpuClock(adoptedThread.get());
        adoptedThread->threadId = std::this_thread::get_id();
        adoptedThread->hostThread = pthread_self();
        SetStackFromHost(adoptedThread.get());
        CurrentGuestState().current = adoptedThread.get();
    }
#endif
    return CurrentGuestState().current;
}

void APS5_VABI scePthreadYield() {
#ifdef _WIN32
    thread_local unsigned int spins = 0;
    thread_local std::uint64_t lastTsc = 0;
    const auto now = __rdtsc();
    if (now - lastTsc > 10000) {
        spins = 0;
    }
    lastTsc = now;
    if (SwitchToThread()) {
        spins = 0;
        return;
    }
    if (++spins < 512) {
        _mm_pause();
        return;
    }
    spins = 0;
    Sleep(1);
    lastTsc = __rdtsc();
#else
    std::this_thread::yield();
#endif
}

int APS5_VABI scePthreadCancel(Pthread thread) {
    if (!thread) return SCE_KERNEL_ERROR_ESRCH;
    thread->cancelPending.store(true);
    for (;;) {
        {
            std::lock_guard lock(thread->cancelLock);
            if (!thread->cancelWait) return SCE_OK;
            thread->cancelWait->NotifyAll();
        }
        scePthreadYield();

    }
}

void APS5_VABI __pthread_cleanup_push_imp_nid_postfix(void (APS5_VABI* routine)(void*), void* argument, void* info) {
    auto* handler = static_cast<CleanupHandler*>(info);
    handler->routine = routine;
    handler->argument = argument;
    handler->onHeap = 0;
    handler->previous = CurrentGuestState().cleanupHandlers;
    CurrentGuestState().cleanupHandlers = handler;
}

void APS5_VABI __pthread_cleanup_pop_imp_nid_postfix(int execute) {
    auto* handler = CurrentGuestState().cleanupHandlers;
    if (!handler) return;
    CurrentGuestState().cleanupHandlers = handler->previous;
    if (execute) handler->routine(handler->argument);
    if (handler->onHeap) delete handler;
}

void APS5_VABI pthread_cleanup_push_nid_postfix(void (APS5_VABI* routine)(void*), void* argument) {
    auto* handler = new CleanupHandler{CurrentGuestState().cleanupHandlers, routine, argument, 1};
    CurrentGuestState().cleanupHandlers = handler;
}

void APS5_VABI pthread_cleanup_pop_nid_postfix(int execute) {
    __pthread_cleanup_pop_imp_nid_postfix(execute);

}

int APS5_VABI scePthreadEqual(Pthread thread1, Pthread thread2) {
    return thread1 == thread2 ? 1 : 0;
}

KernelCpumask APS5_VABI sceKernelGetAvailableCpumask(void) {
    return DEFAULT_THREAD_AFFINITY;
}

int APS5_VABI scePthreadGetaffinity(Pthread thread, KernelCpumask* mask) {
    if (!thread || !mask) return SCE_KERNEL_ERROR_EINVAL;
    *mask = thread->affinity.load(std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadGetname(Pthread thread, char* name) {
    if (!thread || !name) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(thread->nameLock);
    const std::size_t length = std::min<std::size_t>(thread->name.size(), THREAD_NAME_CAPACITY - 1);
    std::memcpy(name, thread->name.data(), length);
    name[length] = '\0';
    return SCE_OK;
}

int APS5_VABI scePthreadGetprio(Pthread thread, int* prio) {
    if (!thread || !prio) return SCE_KERNEL_ERROR_EINVAL;
    *prio = thread->priority.load(std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadGetthreadid(void) {
    return static_cast<int>(scePthreadSelf()->tid);
}

int APS5_VABI scePthreadRename(Pthread thread, const char* name) {
    if (!thread || !name) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(thread->nameLock);
    thread->name = name;
#ifdef _WIN32
    const std::wstring description(thread->name.begin(), thread->name.end());
    SetThreadDescription(static_cast<HANDLE>(thread->nativeHandle), description.c_str());
    ApplyJobAffinity(*thread, thread->nativeHandle);
#endif
    return SCE_OK;
}

int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask) {
    if (!thread || mask == 0) return SCE_KERNEL_ERROR_EINVAL;
    thread->affinity.store(mask, std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadSetcancelstate(int state, int* old_state) {
    if (state != GUEST_CANCEL_ENABLE && state != GUEST_CANCEL_DISABLE) return SCE_KERNEL_ERROR_EINVAL;
    if (old_state) *old_state = CurrentGuestState().cancelState;
    CurrentGuestState().cancelState = state;
    if (CurrentGuestState().cancelType == GUEST_CANCEL_ASYNCHRONOUS) ThreadCancel::Check();
    return SCE_OK;
}

int APS5_VABI scePthreadSetcanceltype(int type, int* old_type) {
    if (type != GUEST_CANCEL_DEFERRED && type != GUEST_CANCEL_ASYNCHRONOUS) return SCE_KERNEL_ERROR_EINVAL;
    if (old_type) *old_type = CurrentGuestState().cancelType;
    CurrentGuestState().cancelType = type;
    if (CurrentGuestState().cancelType == GUEST_CANCEL_ASYNCHRONOUS) ThreadCancel::Check();
    return SCE_OK;
}

void APS5_VABI scePthreadTestcancel() {
    ThreadCancel::Check();
}

int APS5_VABI scePthreadSetprio(Pthread thread, int prio) {
    if (!thread) return SCE_KERNEL_ERROR_EINVAL;
    thread->priority.store(prio, std::memory_order_relaxed);
    return SCE_OK;
}

static void APS5_VABI ResetOnce(void* once) {
    constexpr int32_t Never = 0;
    std::atomic_ref<int32_t> state(*static_cast<int32_t*>(once));
    state.store(Never, std::memory_order_release);
    state.notify_all();
}

int APS5_VABI scePthreadOnce(int32_t* once, void (APS5_VABI* init)(void)) {
    if (!once || !init) return SCE_KERNEL_ERROR_EINVAL;
    constexpr int32_t Never = 0;
    constexpr int32_t Done = 1;
    constexpr int32_t Running = 2;
    std::atomic_ref<int32_t> state(*once);
    for (int32_t expected = Never; !state.compare_exchange_strong(expected, Running, std::memory_order_acq_rel); expected = Never) {
        if (expected != Running) return SCE_OK;
        state.wait(Running, std::memory_order_acquire);
    }
    CleanupHandler reset{};
    void* const mark = ThreadCancel::CleanupMark();
    __pthread_cleanup_push_imp_nid_postfix(ResetOnce, once, &reset);

    try {
        init();
    } catch (...) {
        ThreadCancel::CleanupRestore(mark);
        ResetOnce(once);
        throw;
    }
    ThreadCancel::CleanupRestore(mark);
    state.store(Done, std::memory_order_release);
    state.notify_all();
    return SCE_OK;
}

}

extern "C" {

void APS5_VABI __pthread_cxa_finalize_nid_postfix(void* argument) {
    CxaFinalize_nid_no_patch(argument);
}

}
