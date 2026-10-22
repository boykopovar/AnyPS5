#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "prx/libc/include/GuestArena.hpp"
#include <vector>
#else
#include <unistd.h>
#endif
#include "SceTypes.hpp"
#include "prx/libc/include/GuestStacks.hpp"
#include "prx/libc/include/specifics/itanium/UnwindAbi.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/System/include/UserContext.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
int APS5_VABI scePthreadAttrGetguardsize(const PthreadAttr* attr, std::size_t* guardSize);
int APS5_VABI scePthreadAttrGetstack(const PthreadAttr* attr, void** stackAddress, std::size_t* stackSize);
int APS5_VABI scePthreadAttrSetguardsize(PthreadAttr* attr, std::size_t guardSize);
int APS5_VABI scePthreadAttrSetstack(PthreadAttr* attr, void* stackAddress, std::size_t stackSize);
int APS5_VABI scePthreadAttrSetstackaddr(PthreadAttr* attr, void* stackAddress);
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stackSize);
int APS5_VABI scePthreadAttrSetdetachstate(PthreadAttr* attr, int detachstate);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadDetach(Pthread thread);
int APS5_VABI scePthreadCancel(Pthread thread);
void APS5_VABI scePthreadExit(void* retval);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor);
void* APS5_VABI scePthreadGetspecific(PthreadKey key);
int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value);
int APS5_VABI sceKernelUsleep_nid_postfix(KernelUseconds microseconds);
int APS5_VABI pthread_attr_setstack_nid_postfix(PthreadAttr* attr, void* stackAddress, std::size_t stackSize);
int APS5_VABI pthread_attr_get_np_nid_postfix(Pthread thread, PthreadAttr* attr);
int APS5_VABI pthread_attr_getguardsize_nid_postfix(const PthreadAttr* attr, std::size_t* guardSize);
int APS5_VABI pthread_attr_getstack_nid_postfix(const PthreadAttr* attr, void** stackAddress, std::size_t* stackSize);
int APS5_VABI pthread_create_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg);
int APS5_VABI pthread_join_nid_postfix(Pthread thread, void** value);
void* APS5_VABI _Thrd_current_nid_postfix(void);
void* APS5_VABI _Tss_get_nid_postfix(int key);
int APS5_VABI _Tss_set_nid_postfix(int key, void* value);
[[noreturn]] void APS5_VABI _Thrd_exit_nid_postfix(int code);
[[gnu::returns_twice]] int APS5_VABI getcontext_nid_postfix(UserUcontext* context);
int APS5_VABI setcontext_nid_postfix(const UserUcontext* context);
void APS5_VABI makecontext_nid_postfix(UserUcontext* context, void (APS5_VABI* entry)(), int count, ...);
void* APS5_VABI Aps5CallGuestEntry_nid_no_patch(PthreadEntry entry, void* arg, std::uintptr_t stackTop);
_Unwind_Reason_Code APS5_VABI _Unwind_Backtrace_nid_postfix(_Unwind_Trace_Fn trace, void* argument);
_Unwind_Ptr APS5_VABI _Unwind_GetIP_nid_postfix(_Unwind_Context* context);
_Unwind_Word APS5_VABI _Unwind_GetCFA_nid_postfix(_Unwind_Context* context);
#ifdef _WIN32
int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t start, std::int64_t end, std::size_t length, std::size_t alignment, int type, std::int64_t* physical);
int APS5_VABI sceKernelMapDirectMemory(void** address, std::size_t length, int protection, int flags, std::int64_t physical, std::size_t alignment);
int APS5_VABI sceKernelMunmap(void* address, std::size_t length);
int APS5_VABI sceKernelReleaseDirectMemory(std::int64_t physical, std::size_t length);
#endif
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ESRCH = static_cast<int>(0x80020003);
static constexpr int DETACH_DETACHED = 1;
static constexpr std::size_t MINIMUM_STACK = 16384;
static constexpr std::size_t STACK_SIZES[] = {MINIMUM_STACK, 65536, 1u << 20};
static constexpr std::size_t CONTEXT_STACK = 256 * 1024;
static constexpr int LIBC_EXIT_CODE = 0x5ce;
static constexpr int CLEANUP_ORDER[] = {3, 5, 4, 1};

thread_local unsigned char threadLocalBlock[512 * 1024];
static PthreadKey key = 0;

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "guest pthread provided stack: %s\n", what);
    std::abort();
}

struct CallerStack {
    std::unique_ptr<unsigned char[]> memory;
    void* base;
    std::size_t size;

    explicit CallerStack(std::size_t bytes, std::size_t offset = 0) : memory(new unsigned char[bytes + offset]), base(memory.get() + offset), size(bytes) {}
    CallerStack(void* address, std::size_t bytes) : base(address), size(bytes) {}

    bool Contains(std::uintptr_t address) const {
        const auto start = reinterpret_cast<std::uintptr_t>(base);
        return address >= start && address - start < size;
    }

    void RequireWritable() const {
        std::memset(base, 0x5a, size);
        auto* bytes = static_cast<const volatile unsigned char*>(base);
        for (std::size_t offset = 0; offset < size; ++offset)
            Require(bytes[offset] == 0x5a, "every byte of the caller's stack stays writable and readable after the thread ends");
    }
};

static bool OnCallerBounds(const CallerStack& stack) {
    const bool framed = stack.Contains(reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)));
#ifdef _WIN32
    ULONG_PTR low = 0;
    ULONG_PTR high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    return framed && low == reinterpret_cast<ULONG_PTR>(stack.base) && high == low + stack.size;
#else
    return framed;
#endif
}

enum class Setter { SceStack, SceAddressAndSize, PosixStack };
enum class Exit { Return, Sce, Libc };

struct Worker {
    const CallerStack* stack = nullptr;
    const CallerStack* detour = nullptr;
    Setter setter = Setter::SceStack;
    Exit exit = Exit::Return;
    std::uintptr_t local = 0;
#ifdef _WIN32
    void* hostThread = nullptr;
#else
    int hostTid = 0;
#endif
    int cleanups = 0;
    int order[std::size(CLEANUP_ORDER)] = {};
    Pthread self = nullptr;
    std::uintptr_t destructorLocal = 0;
    int destructors = 0;
    bool destructorIdentity = false;
    std::atomic<bool> ready{false};

    Worker() = default;
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    ~Worker() {
#ifdef _WIN32
        if (hostThread) CloseHandle(hostThread);
#endif
    }
};

static void RequireReported(Pthread thread, const CallerStack& stack, bool live) {
    PthreadAttr attr = nullptr;
    void* address = nullptr;
    std::size_t size = 0;
    std::size_t guard = 0;
    Require(scePthreadAttrInit(&attr) == SCE_OK && scePthreadAttrSetguardsize(&attr, 8192) == SCE_OK, "initialize a query attribute with a guard");
    if (live) {
        Require(scePthreadAttrGet(thread, &attr) == SCE_OK, "scePthreadAttrGet on a live thread with a caller stack");
        Require(scePthreadAttrGetstack(&attr, &address, &size) == SCE_OK && address == stack.base && size == stack.size, "scePthreadAttrGet reports the caller's base and size unchanged");
        Require(scePthreadAttrGetguardsize(&attr, &guard) == SCE_OK && guard == 0, "scePthreadAttrGet reports no guard for a caller stack");
        Require(scePthreadAttrSetguardsize(&attr, 12288) == SCE_OK, "preseed the query attribute's guard again");
        address = nullptr;
        size = 0;
    } else {
        Require(scePthreadAttrGet(thread, &attr) == SCE_KERNEL_ERROR_ESRCH, "scePthreadAttrGet answers ESRCH for a joined thread with a caller stack");
    }
    Require(pthread_attr_get_np_nid_postfix(thread, &attr) == 0, "pthread_attr_get_np on a thread with a caller stack");
    Require(pthread_attr_getstack_nid_postfix(&attr, &address, &size) == 0 && address == stack.base && size == stack.size, "pthread_attr_get_np reports the caller's base and size unchanged");
    if (live) Require(pthread_attr_getguardsize_nid_postfix(&attr, &guard) == 0 && guard == 0, "pthread_attr_get_np reports no guard for a caller stack");
    Require(scePthreadAttrDestroy(&attr) == SCE_OK, "destroy the query attribute");
}

[[gnu::noinline]] static bool ChainEndsOnStack(const CallerStack& stack) {
    auto* frame = static_cast<std::uintptr_t*>(__builtin_frame_address(0));
    for (int depth = 0; depth < 100; ++depth) {
        const auto address = reinterpret_cast<std::uintptr_t>(frame);
        if (!stack.Contains(address) || !stack.Contains(address + 2 * sizeof(std::uintptr_t) - 1)) return false;
        if (frame[1] == 0) return frame[0] == 0;
        if (frame[0] <= address) return false;
        frame = reinterpret_cast<std::uintptr_t*>(frame[0]);
    }
    return false;
}

static void WaitHostThreadGone(const Worker& worker) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!worker.ready.load(std::memory_order_acquire)) {
        Require(std::chrono::steady_clock::now() < deadline, "a detached worker starts on the caller's stack");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#ifdef _WIN32
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
    Require(WaitForSingleObject(worker.hostThread, static_cast<DWORD>(std::max<long long>(remaining, 0))) == WAIT_OBJECT_0, "a detached worker's host thread ends");
#else
    const auto task = "/proc/self/task/" + std::to_string(worker.hostTid);
    while (access(task.c_str(), F_OK) == 0) {
        Require(std::chrono::steady_clock::now() < deadline, "a detached worker's host thread ends");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#endif
}

static void PublishHostThread(Worker& worker) {
#ifdef _WIN32
    Require(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &worker.hostThread, SYNCHRONIZE, FALSE, 0) != 0, "publish the worker's native thread handle");
#else
    worker.hostTid = static_cast<int>(gettid());
#endif
}

static void APS5_VABI DestroyValue(void* opaque) {
    auto& worker = *static_cast<Worker*>(opaque);
    volatile unsigned char local = 0x24;
    Require(OnCallerBounds(*worker.stack), "the key destructor runs inside the caller's exact native stack bounds");
    worker.destructorLocal = reinterpret_cast<std::uintptr_t>(&local);
    worker.destructorIdentity = scePthreadSelf() == worker.self && scePthreadGetspecific(key) == nullptr && local == 0x24;
    ++worker.destructors;
}

static void BindKey(Worker& worker, void* opaque) {
    worker.self = scePthreadSelf();
    if (worker.exit != Exit::Libc) {
        Require(scePthreadSetspecific(key, opaque) == SCE_OK, "bind the worker to its key on the caller's stack");
        return;
    }
    Require(_Thrd_current_nid_postfix() == worker.self, "_Thrd_current forwards to the thread on the caller's stack");
    Require(_Tss_set_nid_postfix(key, opaque) == 0 && _Tss_get_nid_postfix(key) == opaque, "_Tss_set and _Tss_get forward on the caller's stack");
}

struct CleanupTag {
    Worker* worker;
    int value;
};

static void APS5_VABI RecordCleanup(void* opaque) {
    auto& tag = *static_cast<CleanupTag*>(opaque);
    Require(OnCallerBounds(*tag.worker->stack), "a cleanup handler runs inside the caller's exact native stack bounds");
    if (tag.worker->cleanups < static_cast<int>(std::size(CLEANUP_ORDER))) tag.worker->order[tag.worker->cleanups] = tag.value;
    ++tag.worker->cleanups;
}

static void PushMixedCleanups(Worker& worker, CleanupTag (&tags)[5], CleanupHandler (&infos)[2]) {
    for (int index = 0; index < 5; ++index) tags[index] = {&worker, index + 1};
    __pthread_cleanup_push_imp_nid_postfix(RecordCleanup, &tags[0], &infos[0]);
    pthread_cleanup_push_nid_postfix(RecordCleanup, &tags[1]);
    pthread_cleanup_pop_nid_postfix(0);
    pthread_cleanup_push_nid_postfix(RecordCleanup, &tags[2]);
    __pthread_cleanup_pop_imp_nid_postfix(1);
    pthread_cleanup_push_nid_postfix(RecordCleanup, &tags[3]);
    __pthread_cleanup_push_imp_nid_postfix(RecordCleanup, &tags[4], &infos[1]);
    pthread_cleanup_pop_nid_postfix(1);
}

static void RequireCleanups(const Worker& worker) {
    Require(worker.cleanups == static_cast<int>(std::size(CLEANUP_ORDER)), "the stack and heap cleanup handlers left on a caller stack ran exactly once each");
    Require(std::equal(std::begin(CLEANUP_ORDER), std::end(CLEANUP_ORDER), worker.order), "the stack and heap cleanup handlers ran in LIFO order across both push and pop interfaces");
}

static void Detour(Worker& worker);

static void* APS5_VABI Entry(void* opaque) {
    auto& worker = *static_cast<Worker*>(opaque);
    threadLocalBlock[0] = 1;
    threadLocalBlock[sizeof(threadLocalBlock) - 1] = 2;
    volatile unsigned char local = 0x42;
    worker.local = reinterpret_cast<std::uintptr_t>(&local);
    Require(OnCallerBounds(*worker.stack), "the entry runs inside the caller's exact native stack bounds");
    BindKey(worker, opaque);
    RequireReported(worker.self, *worker.stack, true);
    Require(ChainEndsOnStack(*worker.stack), "the frame chain stays on the caller's stack and ends at a zero record");
    Require(threadLocalBlock[0] == 1 && threadLocalBlock[sizeof(threadLocalBlock) - 1] == 2 && local == 0x42, "static TLS and the entry's locals survive on a caller stack");
    if (worker.detour) Detour(worker);
    PublishHostThread(worker);
    worker.ready.store(true, std::memory_order_release);
    if (worker.exit == Exit::Return) return opaque;
    CleanupTag tags[5];
    CleanupHandler infos[2]{};
    PushMixedCleanups(worker, tags, infos);
    if (worker.exit == Exit::Sce) scePthreadExit(opaque);
    _Thrd_exit_nid_postfix(LIBC_EXIT_CODE);
}

static void* APS5_VABI CancelEntry(void* opaque) {
    auto& worker = *static_cast<Worker*>(opaque);
    CleanupTag tags[5];
    CleanupHandler infos[2]{};
    PushMixedCleanups(worker, tags, infos);
    volatile unsigned char local = 0;
    worker.local = reinterpret_cast<std::uintptr_t>(&local);
    Require(OnCallerBounds(*worker.stack), "the cancelable entry runs inside the caller's exact native stack bounds");
    BindKey(worker, opaque);
    worker.ready.store(true, std::memory_order_release);
    for (;;) sceKernelUsleep_nid_postfix(1000);
}

static Pthread Start(Worker& worker, PthreadEntry entry, bool detached) {
    const auto& stack = *worker.stack;
    PthreadAttr attr = nullptr;
    std::size_t guard = 0;
    Require(scePthreadAttrInit(&attr) == SCE_OK && scePthreadAttrSetguardsize(&attr, 65536) == SCE_OK, "initialize the creation attribute with a guard");
    switch (worker.setter) {
    case Setter::SceStack:
        Require(scePthreadAttrSetstack(&attr, stack.base, stack.size) == SCE_OK, "scePthreadAttrSetstack accepts the caller's stack");
        break;
    case Setter::SceAddressAndSize:
        Require(scePthreadAttrSetstackaddr(&attr, stack.base) == SCE_OK, "scePthreadAttrSetstackaddr accepts the caller's address");
        Require(scePthreadAttrSetstacksize(&attr, stack.size) == SCE_OK, "scePthreadAttrSetstacksize accepts the caller's size");
        break;
    case Setter::PosixStack:
        Require(pthread_attr_setstack_nid_postfix(&attr, stack.base, stack.size) == 0, "pthread_attr_setstack accepts the caller's stack");
        break;
    }
    if (detached) Require(scePthreadAttrSetdetachstate(&attr, DETACH_DETACHED) == SCE_OK, "request a detached thread");
    Pthread thread = nullptr;
    if (worker.setter == Setter::PosixStack)
        Require(pthread_create_nid_postfix(&thread, &attr, entry, &worker) == 0, "pthread_create on the caller's stack");
    else
        Require(scePthreadCreate(&thread, &attr, entry, &worker, "provided stack") == SCE_OK, "scePthreadCreate on the caller's stack");
    Require(scePthreadAttrGetguardsize(&attr, &guard) == SCE_OK && guard == 65536, "the creation attribute keeps its configured guard after create");
    Require(scePthreadAttrDestroy(&attr) == SCE_OK, "destroy the creation attribute right after create");
    return thread;
}

static void* Join(const Worker& worker, Pthread thread) {
    void* result = nullptr;
    if (worker.setter == Setter::PosixStack)
        Require(pthread_join_nid_postfix(thread, &result) == 0, "pthread_join a thread on the caller's stack");
    else
        Require(scePthreadJoin(thread, &result) == SCE_OK, "scePthreadJoin a thread on the caller's stack");
    return result;
}

static void RequireDestroyed(const Worker& worker, const CallerStack& stack) {
    Require(worker.destructors == 1, "the key destructor ran once for a thread on the caller's stack");
    Require(worker.destructorIdentity, "the key destructor ran on the ending thread with its value cleared");
    Require(stack.Contains(worker.destructorLocal), "the key destructor's local lies inside the caller's stack");
}

static void RunJoined(const CallerStack& stack, Setter setter, Exit exit, const CallerStack* detour = nullptr) {
    Worker worker;
    worker.stack = &stack;
    worker.detour = detour;
    worker.setter = setter;
    worker.exit = exit;
    Pthread thread = Start(worker, Entry, false);
    void* const expected = exit == Exit::Libc ? reinterpret_cast<void*>(std::uintptr_t{LIBC_EXIT_CODE}) : &worker;
    Require(Join(worker, thread) == expected, "the entry's return value, scePthreadExit value or _Thrd_exit code reaches join");
    Require(stack.Contains(worker.local), "the entry's local lies inside the caller's stack");
    if (exit == Exit::Return) Require(worker.cleanups == 0, "a returning entry runs no cleanup handler");
    else RequireCleanups(worker);
    RequireDestroyed(worker, stack);
    RequireReported(thread, stack, false);
    stack.RequireWritable();
}

static void RunDetached(const CallerStack& stack, Setter setter, bool detachAtCreate) {
    Worker worker;
    worker.stack = &stack;
    worker.setter = setter;
    Pthread thread = Start(worker, Entry, detachAtCreate);
    if (!detachAtCreate) Require(scePthreadDetach(thread) == SCE_OK, "scePthreadDetach a thread on the caller's stack");
    WaitHostThreadGone(worker);
    Require(stack.Contains(worker.local), "a detached entry's local lies inside the caller's stack");
    RequireDestroyed(worker, stack);
    stack.RequireWritable();
}

static void RunCanceled(const CallerStack& stack, Setter setter) {
    Worker worker;
    worker.stack = &stack;
    worker.setter = setter;
    Pthread thread = Start(worker, CancelEntry, false);
    while (!worker.ready.load(std::memory_order_acquire)) std::this_thread::yield();
    Require(scePthreadCancel(thread) == SCE_OK, "cancel a thread on the caller's stack");
    Require(Join(worker, thread) == GUEST_CANCELED, "a canceled thread on the caller's stack joins with PTHREAD_CANCELED");
    RequireCleanups(worker);
    Require(stack.Contains(worker.local), "the canceled entry's local lies inside the caller's stack");
    RequireDestroyed(worker, stack);
    stack.RequireWritable();
}

static void RunConcurrent() {
    constexpr std::size_t Count = 4;
    std::unique_ptr<CallerStack> stacks[Count];
    Worker workers[Count];
    Pthread threads[Count];
    for (std::size_t index = 0; index < Count; ++index) {
        stacks[index] = std::make_unique<CallerStack>(65536);
        workers[index].stack = stacks[index].get();
        workers[index].setter = static_cast<Setter>(index % 3);
        threads[index] = Start(workers[index], Entry, false);
    }
    for (std::size_t index = 0; index < Count; ++index) {
        Require(Join(workers[index], threads[index]) == &workers[index], "concurrent threads on caller stacks join with their own values");
        for (std::size_t other = 0; other < Count; ++other)
            Require(stacks[other]->Contains(workers[index].local) == (other == index), "concurrent threads run on their own caller stacks only");
        RequireDestroyed(workers[index], *stacks[index]);
        stacks[index]->RequireWritable();
    }
}

static void RunOverflowRefused() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK, "initialize the overflow attribute");
    auto* wrapping = reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - MINIMUM_STACK + 2);
    Require(scePthreadAttrSetstack(&attr, wrapping, MINIMUM_STACK) == SCE_OK, "the setter keeps accepting any non-null address");
    Worker worker;
    Pthread thread = nullptr;
    bool threw = false;
    try {
        scePthreadCreate(&thread, &attr, Entry, &worker, nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw && thread == nullptr, "a caller stack whose end wraps around the address space is refused at create");
    Require(scePthreadAttrDestroy(&attr) == SCE_OK, "destroy the overflow attribute");
}

#ifdef _WIN32
static std::size_t CollectSharedWrites(const CallerStack& stack) {
    std::vector<void*> pages(stack.size / 4096 + 1);
    std::size_t count = pages.size();
    Require(GuestArena::GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(stack.base), stack.size, pages.data(), &count, true), "collect the writes to the shared stack");
    return count;
}

static bool SharedPagesProtected(const CallerStack& stack, DWORD protection) {
    for (std::size_t offset = 0; offset < stack.size; offset += 4096) {
        MEMORY_BASIC_INFORMATION info{};
        Require(VirtualQuery(static_cast<unsigned char*>(stack.base) + offset, &info, sizeof(info)) == sizeof(info), "query a page of the shared stack");
        if (info.Protect != protection) return false;
    }
    return true;
}

static bool SharedPagesPinned(const CallerStack& stack) {
    CollectSharedWrites(stack);
    return SharedPagesProtected(stack, PAGE_READWRITE);
}

static bool SharedPagesRearmed(const CallerStack& stack) {
    Require(CollectSharedWrites(stack) == stack.size / 4096, "the collector reports every page of the unpinned shared stack before arming it");
    return CollectSharedWrites(stack) == 0 && SharedPagesProtected(stack, PAGE_READONLY);
}

static void RunPinLifecycle(const CallerStack& stack) {
    Require(SharedPagesRearmed(stack), "fresh shared pages arm read-only before any thread used them as a caller stack");
    Worker worker;
    worker.stack = &stack;
    Pthread thread = Start(worker, CancelEntry, false);
    while (!worker.ready.load(std::memory_order_acquire)) std::this_thread::yield();
    Require(SharedPagesPinned(stack), "a live thread keeps the shared pages of its caller stack writable through a collection");
    Require(scePthreadCancel(thread) == SCE_OK, "cancel the live thread on the shared stack");
    Require(Join(worker, thread) == GUEST_CANCELED, "join the canceled thread on the shared stack after its native thread ended");
    RequireCleanups(worker);
    RequireDestroyed(worker, stack);
    stack.RequireWritable();
    Require(SharedPagesRearmed(stack), "the shared pages arm read-only again after the canceled native thread ended");
    RunJoined(stack, Setter::SceAddressAndSize, Exit::Sce);
    Require(SharedPagesRearmed(stack), "the shared pages arm read-only again after scePthreadExit ended the native thread");
    RunJoined(stack, Setter::PosixStack, Exit::Libc);
    Require(SharedPagesRearmed(stack), "the shared pages arm read-only again after _Thrd_exit ended the native thread");
    RunJoined(stack, Setter::SceStack, Exit::Return);
    Require(SharedPagesRearmed(stack), "the shared pages arm read-only again after a returning entry ended the native thread");
}
#endif

static const CallerStack* contextStack = nullptr;
static std::uintptr_t contextLocal = 0;
static bool contextBounds = false;
static int contextEntries = 0;

static void APS5_VABI ContextEntry() {
    volatile unsigned char local = 0x7e;
    contextLocal = reinterpret_cast<std::uintptr_t>(&local);
    contextBounds = OnCallerBounds(*contextStack) && local == 0x7e;
    ++contextEntries;
}

static void Detour(Worker& worker) {
    UserUcontext context{};
    UserUcontext back{};
    volatile int phase = 0;
    const int entries = contextEntries;
    Require(OnCallerBounds(*worker.stack), "the worker starts its detour from its exact caller bounds");
    Require(getcontext_nid_postfix(&back) == 0, "capture the worker before its detour");
    if (phase == 0) {
        phase = 1;
        Require(getcontext_nid_postfix(&context) == 0, "capture the detour template");
        context.stackPointer = worker.detour->base;
        context.stackSize = worker.detour->size;
        context.link = &back;
        contextStack = worker.detour;
        makecontext_nid_postfix(&context, &ContextEntry, 0);
        Require(context.mcontext.len == sizeof(UserMcontext), "makecontext accepts the detour stack");
        setcontext_nid_postfix(&context);
        Require(false, "setcontext into the detour returned");
    }
    Require(contextEntries == entries + 1 && worker.detour->Contains(contextLocal) && !worker.stack->Contains(contextLocal) && contextBounds, "the detour ran once on its own stack, clear of the worker's caller stack, with its own bounds");
    Require(scePthreadSelf() == worker.self, "uc_link resumes the detour's worker on the same thread");
    Require(OnCallerBounds(*worker.stack), "uc_link restores the worker's exact caller bounds rather than the larger registered range");
}

static void RunInsideContextStack(const CallerStack& outer) {
    const CallerStack detour(CONTEXT_STACK);
    UserUcontext context{};
    UserUcontext caller{};
    volatile int phase = 0;
    Require(getcontext_nid_postfix(&caller) == 0, "capture the caller of the context");
    if (phase == 0) {
        phase = 1;
        Require(getcontext_nid_postfix(&context) == 0, "capture the context template");
        context.stackPointer = outer.base;
        context.stackSize = outer.size;
        context.link = &caller;
        makecontext_nid_postfix(&context, &ContextEntry, 0);
        Require(context.mcontext.len == sizeof(UserMcontext), "makecontext accepts the context stack");
#ifdef _WIN32
        Require(SharedPagesPinned(outer), "makecontext pins the shared pages of its stack");
#endif
        const CallerStack inner(static_cast<unsigned char*>(outer.base) + outer.size / 4, outer.size / 4);
        RunJoined(inner, Setter::SceStack, Exit::Return);
        RunJoined(inner, Setter::PosixStack, Exit::Sce);
        RunJoined(inner, Setter::SceAddressAndSize, Exit::Sce, &detour);
        RunJoined(inner, Setter::PosixStack, Exit::Libc, &outer);
#ifdef _WIN32
        Require(SharedPagesPinned(outer), "threads on part of a context stack leave its pages pinned");
#endif
        contextStack = &outer;
        setcontext_nid_postfix(&context);
        Require(false, "setcontext into the context stack returned");
    }
    Require(outer.Contains(contextLocal), "the context runs on its stack after threads used part of it");
    Require(contextBounds, "the context keeps its stack bounds after threads used part of its stack");
}

static std::uintptr_t helperLocal = 0;
#ifdef _WIN32
static ULONG_PTR helperLow = 0;
static ULONG_PTR helperHigh = 0;
#endif

static void* APS5_VABI RecordBounds(void* opaque) {
    volatile unsigned char local = 0x19;
    helperLocal = reinterpret_cast<std::uintptr_t>(&local);
#ifdef _WIN32
    GetCurrentThreadStackLimits(&helperLow, &helperHigh);
#endif
    return local == 0x19 ? opaque : nullptr;
}

static void RunHostBoundsRestored(const CallerStack& stack) {
    int marker = 0;
#ifdef _WIN32
    ULONG_PTR low = 0;
    ULONG_PTR high = 0;
    GetCurrentThreadStackLimits(&low, &high);
#endif
    GuestStacks::GuestStackEnter_nid_no_patch(stack.base, stack.size);
    void* const result = Aps5CallGuestEntry_nid_no_patch(RecordBounds, &marker, reinterpret_cast<std::uintptr_t>(stack.base) + stack.size);
    GuestStacks::GuestStackLeave_nid_no_patch();
    Require(result == &marker && stack.Contains(helperLocal), "the trampoline runs its entry on the caller's stack and returns its value");
#ifdef _WIN32
    ULONG_PTR restoredLow = 0;
    ULONG_PTR restoredHigh = 0;
    GetCurrentThreadStackLimits(&restoredLow, &restoredHigh);
    Require(helperLow == reinterpret_cast<ULONG_PTR>(stack.base) && helperHigh == helperLow + stack.size, "GuestStackEnter publishes the caller's exact bounds to the trampoline's entry");
    Require(restoredLow == low && restoredHigh == high, "GuestStackLeave restores the host thread's native stack bounds after the trampoline returned");
#endif
}

struct UnwoundFrame {
    std::uintptr_t ip;
    std::uintptr_t cfa;
};

struct Unwound {
    std::uintptr_t trampoline = 0;
    bool caught = false;
    UnwoundFrame frames[64] = {};
    std::size_t count = 0;
    _Unwind_Reason_Code result = _URC_NO_REASON;
};

static _Unwind_Reason_Code APS5_VABI RecordFrame(_Unwind_Context* context, void* opaque) {
    auto& unwound = *static_cast<Unwound*>(opaque);
    if (unwound.count == std::size(unwound.frames)) return _URC_NORMAL_STOP;
    unwound.frames[unwound.count++] = {_Unwind_GetIP_nid_postfix(context), _Unwind_GetCFA_nid_postfix(context)};
    return _URC_NO_REASON;
}

[[gnu::noinline]] static void Backtrace(Unwound& unwound) {
    unwound.result = _Unwind_Backtrace_nid_postfix(RecordFrame, &unwound);
}

static void* APS5_VABI UnwoundEntry(void* opaque) {
    auto& unwound = *static_cast<Unwound*>(opaque);
    unwound.trampoline = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
    try {
        throw std::runtime_error("host exception on a guest thread");
    } catch (const std::runtime_error&) {
        unwound.caught = true;
    }
    Backtrace(unwound);
    return opaque;
}

static bool CfaRises(const UnwoundFrame* begin, const UnwoundFrame* end) {
    return std::adjacent_find(begin, end, [](const UnwoundFrame& callee, const UnwoundFrame& caller) { return caller.cfa <= callee.cfa; }) == end;
}

static void RunUnwound(const CallerStack* stack) {
    Unwound unwound;
    PthreadAttr attr = nullptr;
    Pthread thread = nullptr;
    void* result = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK, "initialize the attribute of the unwound thread");
    if (stack) Require(scePthreadAttrSetstack(&attr, stack->base, stack->size) == SCE_OK, "scePthreadAttrSetstack accepts the unwound thread's caller stack");
    Require(scePthreadCreate(&thread, &attr, UnwoundEntry, &unwound, "unwound") == SCE_OK, "scePthreadCreate starts the unwound thread");
    Require(scePthreadAttrDestroy(&attr) == SCE_OK, "destroy the attribute of the unwound thread");
    Require(scePthreadJoin(thread, &result) == SCE_OK && result == &unwound, "the unwound thread joins with its entry's return value");
    Require(unwound.caught, "a host exception thrown inside the entry is caught there");
    const UnwoundFrame* const begin = unwound.frames;
    const UnwoundFrame* const end = begin + unwound.count;
    Require(unwound.result == _URC_END_OF_STACK, "_Unwind_Backtrace walks the thread until it finds no further caller");
    Require(std::none_of(begin, end, [](const UnwoundFrame& frame) { return frame.ip == 0; }), "_Unwind_Backtrace reports only nonzero instruction pointers");
    Require(unwound.count > 2 && begin[2].ip == unwound.trampoline, "_Unwind_Backtrace reports the walker, then the entry, then the trampoline the entry returns to");
    Require(std::count_if(begin, end, [&](const UnwoundFrame& frame) { return frame.ip == unwound.trampoline; }) == (stack ? 2 : 1), "the walk passes the trampoline once for the entry and once more where it switched onto a caller stack");
    const UnwoundFrame* host = begin;
    if (stack) {
        host = std::find_if(begin, end, [stack](const UnwoundFrame& frame) { return !stack->Contains(frame.cfa); });
        Require(host - begin > 2, "the walker, the entry and the trampoline that called the entry have their CFA on the caller's stack");
        Require(host < end && host->ip == unwound.trampoline, "the first frame whose CFA leaves the caller's stack is the trampoline that switched onto it");
        Require(std::none_of(host, end, [stack](const UnwoundFrame& frame) { return stack->Contains(frame.cfa); }), "no frame beyond the switching trampoline has its CFA on the caller's stack");
    }
    const UnwoundFrame* const last = stack ? host : begin + 2;
    Require(end - last > 1, "the walk recovers the last trampoline's CFA and return address and reports a frame beyond it");
    Require(CfaRises(begin, host) && CfaRises(host, end), "the CFA rises from each frame to its caller on the same stack");
}

int main() {
    Require(scePthreadKeyCreate(&key, reinterpret_cast<pthread_key_destructor_func_t>(DestroyValue)) == SCE_OK, "create the key whose destructor runs on caller stacks");
    for (auto size : STACK_SIZES) {
        CallerStack stack(size);
        RunJoined(stack, Setter::SceStack, Exit::Return);
        RunJoined(stack, Setter::SceAddressAndSize, Exit::Sce);
        RunJoined(stack, Setter::PosixStack, Exit::Return);
        RunJoined(stack, Setter::SceStack, Exit::Sce);
        RunJoined(stack, Setter::PosixStack, Exit::Libc);
    }
    CallerStack unaligned(MINIMUM_STACK + 3, 8);
    RunJoined(unaligned, Setter::SceStack, Exit::Return);
    RunJoined(unaligned, Setter::PosixStack, Exit::Sce);
    RunJoined(unaligned, Setter::SceAddressAndSize, Exit::Libc);
    CallerStack shared(1u << 20);
    RunCanceled(shared, Setter::SceStack);
    RunCanceled(shared, Setter::PosixStack);
    RunDetached(shared, Setter::SceStack, true);
    RunDetached(shared, Setter::SceAddressAndSize, false);
    RunDetached(shared, Setter::PosixStack, true);
    RunJoined(shared, Setter::SceAddressAndSize, Exit::Return);
    RunConcurrent();
    RunHostBoundsRestored(shared);
    RunUnwound(&shared);
    RunUnwound(nullptr);
#ifdef _WIN32
    std::int64_t physical = 0;
    void* mapped = nullptr;
    void* alias = nullptr;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, CONTEXT_STACK, 0, 0, &physical) == SCE_OK, "allocate direct memory for a context stack");
    Require(sceKernelMapDirectMemory(&mapped, CONTEXT_STACK, 3, 0, physical, 0) == SCE_OK && sceKernelMapDirectMemory(&alias, CONTEXT_STACK, 3, 0, physical, 0) == SCE_OK, "map the context stack twice so its pages are shared");
    RunPinLifecycle(CallerStack(mapped, CONTEXT_STACK));
    RunInsideContextStack(CallerStack(mapped, CONTEXT_STACK));
    Require(sceKernelMunmap(alias, CONTEXT_STACK) == SCE_OK && sceKernelMunmap(mapped, CONTEXT_STACK) == SCE_OK && sceKernelReleaseDirectMemory(physical, CONTEXT_STACK) == SCE_OK, "release the context stack");
#else
    RunInsideContextStack(CallerStack(CONTEXT_STACK));
#endif
    RunOverflowRefused();
    std::puts("guest pthread provided stack tests passed");
    return 0;
}
