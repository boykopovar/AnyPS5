#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <stdexcept>
#include <string>
#include <xmmintrin.h>

#ifdef _WIN32
#include <windows.h>
#include "prx/libc/include/GuestArena.hpp"
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr int32_t SCE_OK = 0;
static constexpr int32_t SCE_FIBER_ERROR_NULL = static_cast<int32_t>(0x80590001);
static constexpr int32_t SCE_FIBER_ERROR_ALIGNMENT = static_cast<int32_t>(0x80590002);
static constexpr int32_t SCE_FIBER_ERROR_RANGE = static_cast<int32_t>(0x80590003);
static constexpr int32_t SCE_FIBER_ERROR_INVALID = static_cast<int32_t>(0x80590004);
static constexpr int32_t SCE_FIBER_ERROR_PERMISSION = static_cast<int32_t>(0x80590005);
static constexpr int32_t SCE_FIBER_ERROR_STATE = static_cast<int32_t>(0x80590006);

static constexpr std::size_t FIBER_OBJECT_SIZE = 0x100;
static constexpr std::size_t FIBER_OPT_PARAM_SIZE = 0x80;
static constexpr std::size_t FIBER_MIN_CONTEXT_SIZE = 512;
static constexpr std::uint64_t FIBER_CONTEXT_FILL = 0xdeadbeefdeadbeefull;
static constexpr std::size_t FIBER_HOST_STACK_MARGIN = 1u << 20;
static constexpr std::size_t FIBER_GUEST_STACK_MIN = 64u << 10;
static constexpr std::uint32_t FIBER_FLAG_NO_ULOBJMGR = 0x1;
static constexpr std::uint32_t FIBER_FLAG_CONTEXT_SIZE_CHECK = 0x10;
static constexpr std::uint32_t FIBER_FLAG_SET_FPU_REGISTERS = 0x100;
static constexpr std::uint32_t FIBER_DEFAULT_MXCSR = 0x9fc0;
static constexpr std::uint16_t FIBER_DEFAULT_FPU_CONTROL = 0x037f;

using GuestFiberEntry = FiberEntry;

// A fiber that is switching away stays Suspending until its context is saved; whoever runs next
// on that host thread publishes Suspended. Fibers migrate between threads, so a resumer must never
// see Suspended before the saved stack pointer is valid.
enum class FiberState : std::uint32_t {
    Idle = 1,
    Running = 2,
    Suspending = 3,
    Suspended = 4,
};

struct Fiber {
    std::uint64_t magic;
    std::atomic<FiberState> state;
    std::uint32_t reserved;
    GuestFiberEntry entry;
    std::uint64_t argOnInitialize;
    std::uint8_t* context;
    std::uint64_t contextSize;
    void* savedStack;
    char name[FIBER_MAX_NAME_LENGTH + 1];
    bool contextSizeCheck;
    bool setFpuRegisters;
    std::uint8_t* hostStack;
    std::uint64_t hostStackBytes;
};
static_assert(sizeof(Fiber) <= FIBER_OBJECT_SIZE, "guest reserves 0x100 bytes for SceFiber");

static constexpr std::uint64_t FIBER_MAGIC = 0x5245424946355041ull;

static std::atomic<bool> g_contextSizeCheck{false};

struct StackBounds {
    void* base;
    void* limit;
    void* deallocation;
};

struct ThreadFiberState {
    Fiber* current = nullptr;
    void* threadStack = nullptr;
    std::uint64_t threadFramePointer = 0;
    StackBounds threadBounds{};
    std::uint64_t transfer = 0;
    Fiber* pendingSuspend = nullptr;
};

static thread_local ThreadFiberState g_thread;

// A fiber may resume on a different host thread than the one that suspended it, so the
// thread-local state must be looked up again after every stack switch rather than cached.
__attribute__((noinline)) static ThreadFiberState& ThreadState() {
    asm volatile("" ::: "memory");
    return g_thread;
}

static bool TraceFibers() {
    static const bool enabled = std::getenv("APS5_TRACE_FIBER") != nullptr;
    return enabled;
}

static void CompletePendingSuspend() {
    auto& thread = ThreadState();
    if (thread.pendingSuspend) {
        auto* fiber = thread.pendingSuspend;
        const auto state = fiber->context ? FiberState::Suspended : FiberState::Idle;
        if (!fiber->context) fiber->savedStack = nullptr;
        fiber->state.store(state, std::memory_order_release);
        thread.pendingSuspend = nullptr;
    }
}

#ifdef _WIN32

extern "C" void Aps5FiberSwitchStack_nid_no_patch(void** save, void** load, Fiber* start);
extern "C" void Aps5FiberTrampoline_nid_no_patch();

asm(R"(
    .text
    .globl Aps5FiberSwitchStack_nid_no_patch
    .def Aps5FiberSwitchStack_nid_no_patch; .scl 2; .type 32; .endef
Aps5FiberSwitchStack_nid_no_patch:
    push %rbp
    push %rbx
    push %rdi
    push %rsi
    push %r12
    push %r13
    push %r14
    push %r15
    sub $0xa8, %rsp
    movaps %xmm6, 0x00(%rsp)
    movaps %xmm7, 0x10(%rsp)
    movaps %xmm8, 0x20(%rsp)
    movaps %xmm9, 0x30(%rsp)
    movaps %xmm10, 0x40(%rsp)
    movaps %xmm11, 0x50(%rsp)
    movaps %xmm12, 0x60(%rsp)
    movaps %xmm13, 0x70(%rsp)
    movaps %xmm14, 0x80(%rsp)
    movaps %xmm15, 0x90(%rsp)
    stmxcsr 0xa0(%rsp)
    fnstcw 0xa4(%rsp)
    mov %rsp, (%rcx)
    mov (%rdx), %rsp
    test %r8, %r8
    jz 1f
    mov %r8, %r12
    jmp Aps5FiberTrampoline_nid_no_patch
1:
    movaps 0x00(%rsp), %xmm6
    movaps 0x10(%rsp), %xmm7
    movaps 0x20(%rsp), %xmm8
    movaps 0x30(%rsp), %xmm9
    movaps 0x40(%rsp), %xmm10
    movaps 0x50(%rsp), %xmm11
    movaps 0x60(%rsp), %xmm12
    movaps 0x70(%rsp), %xmm13
    movaps 0x80(%rsp), %xmm14
    movaps 0x90(%rsp), %xmm15
    ldmxcsr 0xa0(%rsp)
    fldcw 0xa4(%rsp)
    add $0xa8, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rsi
    pop %rdi
    pop %rbx
    pop %rbp
    ret

    .globl Aps5FiberTrampoline_nid_no_patch
    .def Aps5FiberTrampoline_nid_no_patch; .scl 2; .type 32; .endef
Aps5FiberTrampoline_nid_no_patch:
    mov %r12, %rcx
    and $-16, %rsp
    push $0
    push $0
    mov %rsp, %rbp
    sub $32, %rsp
    call Aps5FiberMain_nid_no_patch
    ud2
)");

struct InitialFrame {
    std::uint8_t xmm[0xa0];
    std::uint32_t mxcsr;
    std::uint16_t fpuControl;
    std::uint16_t padding;
    std::uint64_t r15, r14, r13, r12, rsi, rdi, rbx, rbp;
    std::uint64_t returnAddress;
};
static_assert(sizeof(InitialFrame) == 0xa8 + 8 * 8 + 8, "initial fiber frame must match Aps5FiberSwitchStack_nid_no_patch");

static StackBounds CurrentBounds() {
    auto* tib = reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    auto* teb = reinterpret_cast<std::uint8_t*>(tib);
    return {tib->StackBase, tib->StackLimit, *reinterpret_cast<void**>(teb + 0x1478)};
}

static void SetBounds(const StackBounds& bounds) {
    auto* tib = reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    auto* teb = reinterpret_cast<std::uint8_t*>(tib);
    tib->StackBase = bounds.base;
    tib->StackLimit = bounds.limit;
    *reinterpret_cast<void**>(teb + 0x1478) = bounds.deallocation;
}

static void PinStack(const void* context, std::uint64_t bytes) {
    GuestArena::GuestArenaPinWritable_nid_postfix(context, static_cast<std::size_t>(bytes));
}

static void UnpinStack(const void* context, std::uint64_t bytes) {
    GuestArena::GuestArenaUnpinWritable_nid_postfix(context, static_cast<std::size_t>(bytes));
}

#else

extern "C" void Aps5FiberSwitchStack_nid_no_patch(void** save, void** load, Fiber* start);
extern "C" void Aps5FiberTrampoline_nid_no_patch();

asm(".text\n"
    APS5_ASM_FUNCTION("Aps5FiberSwitchStack_nid_no_patch") R"(
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    sub $8, %rsp
    stmxcsr 0(%rsp)
    fnstcw 4(%rsp)
    mov %rsp, (%rdi)
    mov (%rsi), %rsp
    test %rdx, %rdx
    jz 1f
    mov %rdx, %r12
    jmp )" APS5_ASM_SYMBOL("Aps5FiberTrampoline_nid_no_patch") R"(
1:
    ldmxcsr 0(%rsp)
    fldcw 4(%rsp)
    add $8, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret
)" APS5_ASM_FUNCTION_END("Aps5FiberSwitchStack_nid_no_patch")
    APS5_ASM_FUNCTION("Aps5FiberTrampoline_nid_no_patch") R"(
    mov %r12, %rdi
    and $-16, %rsp
    push $0
    push $0
    mov %rsp, %rbp
    call )" APS5_ASM_SYMBOL("Aps5FiberMain_nid_no_patch") R"(
    ud2
)" APS5_ASM_FUNCTION_END("Aps5FiberTrampoline_nid_no_patch"));

struct InitialFrame {
    std::uint32_t mxcsr;
    std::uint16_t fpuControl;
    std::uint16_t padding;
    std::uint64_t r15, r14, r13, r12, rbx, rbp;
    std::uint64_t returnAddress;
};
static_assert(sizeof(InitialFrame) == 8 + 6 * 8 + 8, "initial fiber frame must match Aps5FiberSwitchStack_nid_no_patch");

static StackBounds CurrentBounds() {
    return {};
}

static void SetBounds(const StackBounds&) {}

static void PinStack(const void*, std::uint64_t) {}

static void UnpinStack(const void*, std::uint64_t) {}

#endif

#ifdef _WIN32

static std::uint64_t HostStackGuardBytes() {
    static const std::uint64_t bytes = [] {
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        return static_cast<std::uint64_t>(system.dwPageSize);
    }();
    return bytes;
}

static void AllocateHostStack(Fiber* fiber) {
    const auto page = HostStackGuardBytes();
    const auto bytes = (fiber->contextSize + FIBER_HOST_STACK_MARGIN + page - 1) / page * page + page;
    auto* mapping = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (mapping == nullptr) throw std::runtime_error(std::string("sceFiberInitialize: no host stack of ") + std::to_string(bytes) + " bytes for fiber '" + fiber->name + "' (error " + std::to_string(GetLastError()) + ")");
    DWORD previous = 0;
    if (!VirtualProtect(mapping, page, PAGE_NOACCESS, &previous)) {
        const auto error = GetLastError();
        VirtualFree(mapping, 0, MEM_RELEASE);
        throw std::runtime_error(std::string("sceFiberInitialize: cannot protect the guard page of the host stack of fiber '") + fiber->name + "' (error " + std::to_string(error) + ")");
    }
    fiber->hostStack = mapping;
    fiber->hostStackBytes = bytes;
}

static void ReleaseHostStack(Fiber* fiber) {
    if (fiber->hostStack && !VirtualFree(fiber->hostStack, 0, MEM_RELEASE)) throw std::runtime_error(std::string("sceFiberFinalize: cannot release the host stack of fiber '") + fiber->name + "' (error " + std::to_string(GetLastError()) + ")");
    fiber->hostStack = nullptr;
    fiber->hostStackBytes = 0;
}

#else

static void AllocateHostStack(Fiber* fiber) {
    const auto page = static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
    const auto bytes = (fiber->contextSize + FIBER_HOST_STACK_MARGIN + page - 1) / page * page + page;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
#ifdef MAP_STACK
    flags |= MAP_STACK;
#endif
    void* mapping = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (mapping == MAP_FAILED) throw std::runtime_error(std::string("sceFiberInitialize: no host stack of ") + std::to_string(bytes) + " bytes for fiber '" + fiber->name + "' (errno " + std::to_string(errno) + ")");
    if (mprotect(mapping, page, PROT_NONE) != 0) {
        const int error = errno;
        munmap(mapping, bytes);
        throw std::runtime_error(std::string("sceFiberInitialize: cannot protect the guard page of the host stack of fiber '") + fiber->name + "' (errno " + std::to_string(error) + ")");
    }
    fiber->hostStack = static_cast<std::uint8_t*>(mapping);
    fiber->hostStackBytes = bytes;
}

static void ReleaseHostStack(Fiber* fiber) {
    if (fiber->hostStack) munmap(fiber->hostStack, fiber->hostStackBytes);
    fiber->hostStack = nullptr;
    fiber->hostStackBytes = 0;
}

#endif

static std::uint8_t* StackTop(const Fiber* fiber) {
    return fiber->hostStack ? fiber->hostStack + fiber->hostStackBytes : fiber->context + fiber->contextSize;
}

static std::uint64_t* SizedWindow(const Fiber* fiber) {
    if (!fiber->context) return nullptr;
    return reinterpret_cast<std::uint64_t*>(StackTop(fiber) - fiber->contextSize);
}

#ifdef _WIN32

static StackBounds FiberBounds(const Fiber* fiber) {
    if (!fiber->hostStack) return {fiber->context + fiber->contextSize, fiber->context, fiber->context};
    return {fiber->hostStack + fiber->hostStackBytes, fiber->hostStack + HostStackGuardBytes(), fiber->hostStack};
}

#else

static StackBounds FiberBounds(const Fiber* fiber) {
    return {fiber->context + fiber->contextSize, fiber->context, fiber->context};
}

#endif

static Fiber* AsFiber(FiberObject* object) {
    auto* fiber = reinterpret_cast<Fiber*>(object);
    return fiber && fiber->magic == FIBER_MAGIC ? fiber : nullptr;
}

extern "C" [[noreturn]] void Aps5FiberMain_nid_no_patch(Fiber* fiber) {
    CompletePendingSuspend();
    if (!fiber->context && fiber->setFpuRegisters) {
        _mm_setcsr(FIBER_DEFAULT_MXCSR);
        asm volatile("fldcw %0" : : "m"(FIBER_DEFAULT_FPU_CONTROL));
    }
    fiber->entry(fiber->argOnInitialize, ThreadState().transfer);
    throw std::runtime_error(std::string("sceFiber: entry function of fiber '") + fiber->name + "' returned");
}

static void PrepareInitialStack(Fiber* fiber) {
    const auto top = reinterpret_cast<std::uintptr_t>(StackTop(fiber)) & ~static_cast<std::uintptr_t>(15);
    auto* frame = reinterpret_cast<InitialFrame*>(top - 256);
    std::memset(frame, 0, sizeof(*frame));
    std::uint16_t control = FIBER_DEFAULT_FPU_CONTROL;
    if (fiber->setFpuRegisters) {
        frame->mxcsr = FIBER_DEFAULT_MXCSR;
    } else {
        frame->mxcsr = _mm_getcsr();
        asm volatile("fnstcw %0" : "=m"(control));
    }
    frame->fpuControl = control;
    frame->r12 = reinterpret_cast<std::uint64_t>(fiber);
    frame->returnAddress = reinterpret_cast<std::uint64_t>(&Aps5FiberTrampoline_nid_no_patch);
    fiber->savedStack = frame;
}

static bool AcquireForResume(Fiber* target) {
    for (;;) {
        auto state = target->state.load(std::memory_order_acquire);
        if (state == FiberState::Suspending) {
            std::this_thread::yield();
            continue;
        }
        if (state != FiberState::Idle && state != FiberState::Suspended) return false;
        if (target->state.compare_exchange_weak(state, FiberState::Running, std::memory_order_acq_rel)) {
            if (state == FiberState::Idle && target->context) PrepareInitialStack(target);
            return true;
        }
    }
}

static int32_t ValidateContext(const void* addr_context, uint64_t size_context) {
    if (addr_context == nullptr && size_context != 0) return SCE_FIBER_ERROR_INVALID;
    if ((reinterpret_cast<std::uintptr_t>(addr_context) & 15) != 0 || (size_context & 15) != 0) return SCE_FIBER_ERROR_ALIGNMENT;
    if (addr_context != nullptr && size_context < FIBER_MIN_CONTEXT_SIZE) return SCE_FIBER_ERROR_RANGE;
    return SCE_OK;
}

static void AttachStack(Fiber* fiber, void* addr_context, uint64_t size_context) {
    fiber->context = static_cast<std::uint8_t*>(addr_context);
    fiber->contextSize = size_context;
    if (size_context < FIBER_GUEST_STACK_MIN) AllocateHostStack(fiber);
    if (fiber->contextSizeCheck) {
        auto* words = SizedWindow(fiber);
        std::fill(words, words + size_context / sizeof(std::uint64_t), FIBER_CONTEXT_FILL);
    }
    PinStack(addr_context, size_context);
}

static void DetachStack(Fiber* fiber) {
    if (!fiber->context) return;
    UnpinStack(fiber->context, fiber->contextSize);
    ReleaseHostStack(fiber);
}

static int32_t AcquireWithContext(Fiber* target, void* addr_context, uint64_t size_context) {
    if (addr_context == nullptr && size_context == 0) return AcquireForResume(target) ? SCE_OK : SCE_FIBER_ERROR_STATE;
    if (addr_context == nullptr) return SCE_FIBER_ERROR_INVALID;
    if (const auto valid = ValidateContext(addr_context, size_context); valid != SCE_OK) return valid;
    auto expected = FiberState::Idle;
    if (!target->state.compare_exchange_strong(expected, FiberState::Running, std::memory_order_acq_rel)) return SCE_FIBER_ERROR_INVALID;
    if (target->context) {
        target->state.store(FiberState::Idle, std::memory_order_release);
        return SCE_FIBER_ERROR_INVALID;
    }
    AttachStack(target, addr_context, size_context);
    if (TraceFibers()) std::fprintf(stderr, "[fiber] attach %s context=%p+0x%llx\n", target->name, addr_context, static_cast<unsigned long long>(size_context));
    PrepareInitialStack(target);
    return SCE_OK;
}

static void Resume(Fiber* target, void** save, std::uint64_t argOnRun) {
    const auto* frame = static_cast<const InitialFrame*>(target->savedStack);
    if (target->context && frame->returnAddress == 0) {
        std::fprintf(stderr, "[fiber] resuming '%s' with a wiped context: saved=%p context=%p size=0x%llx\n", target->name, target->savedStack,
                     static_cast<void*>(target->context), static_cast<unsigned long long>(target->contextSize));
        const auto* words = static_cast<const std::uint64_t*>(target->savedStack);
        for (int i = 0; i < 40; i += 4) std::fprintf(stderr, "[fiber]   +0x%03x %016llx %016llx %016llx %016llx\n", i * 8, static_cast<unsigned long long>(words[i]), static_cast<unsigned long long>(words[i + 1]), static_cast<unsigned long long>(words[i + 2]), static_cast<unsigned long long>(words[i + 3]));
        std::fflush(stderr);
        std::abort();
    }
    ThreadState().current = target;
    ThreadState().transfer = argOnRun;
    SetBounds(target->context ? FiberBounds(target) : ThreadState().threadBounds);
    Aps5FiberSwitchStack_nid_no_patch(save, target->context ? &target->savedStack : &ThreadState().threadStack, target->context ? nullptr : target);
}

static int32_t InitializeFiber(FiberObject* object, const char* name, GuestFiberEntry entry, uint64_t arg_on_initialize, void* addr_context, uint64_t size_context, uint32_t flags) {
    if (!object || !name || !entry) return SCE_FIBER_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(object) & 7) != 0) return SCE_FIBER_ERROR_ALIGNMENT;
    if (const auto valid = ValidateContext(addr_context, size_context); valid != SCE_OK) return valid;
    if ((flags & ~(FIBER_FLAG_NO_ULOBJMGR | FIBER_FLAG_CONTEXT_SIZE_CHECK | FIBER_FLAG_SET_FPU_REGISTERS)) != 0) throw std::runtime_error("_sceFiberInitializeWithInternalOptionImpl: unsupported flags " + std::to_string(flags));
    auto* fiber = reinterpret_cast<Fiber*>(object);
    if (fiber->magic == FIBER_MAGIC) DetachStack(fiber);
    std::memset(object, 0, FIBER_OBJECT_SIZE);
    fiber->magic = FIBER_MAGIC;
    fiber->state.store(FiberState::Idle, std::memory_order_relaxed);
    fiber->entry = entry;
    fiber->argOnInitialize = arg_on_initialize;
    std::strncpy(fiber->name, name, FIBER_MAX_NAME_LENGTH);
    fiber->contextSizeCheck = g_contextSizeCheck.load(std::memory_order_relaxed) || (flags & FIBER_FLAG_CONTEXT_SIZE_CHECK) != 0;
    fiber->setFpuRegisters = (flags & FIBER_FLAG_SET_FPU_REGISTERS) != 0;
    if (addr_context) AttachStack(fiber, addr_context, size_context);
    if (TraceFibers()) std::fprintf(stderr, "[fiber] init %s object=%p context=%p+0x%llx entry=%p flags=0x%x\n", fiber->name, static_cast<void*>(object), addr_context, static_cast<unsigned long long>(size_context), reinterpret_cast<void*>(entry), flags);
    return SCE_OK;
}

static int32_t RunFiber(FiberObject* object, void* addr_context, uint64_t size_context, uint64_t arg_on_run, uint64_t* arg_on_return, std::uint64_t threadFramePointer) {
    auto* fiber = AsFiber(object);
    if (!fiber) return object ? SCE_FIBER_ERROR_INVALID : SCE_FIBER_ERROR_NULL;
    if (ThreadState().current) return SCE_FIBER_ERROR_PERMISSION;
    if (const auto acquired = AcquireWithContext(fiber, addr_context, size_context); acquired != SCE_OK) return acquired;
    ThreadState().threadFramePointer = threadFramePointer;
    ThreadState().threadBounds = CurrentBounds();
    Resume(fiber, &ThreadState().threadStack, arg_on_run);
    CompletePendingSuspend();
    SetBounds(ThreadState().threadBounds);
    if (arg_on_return) *arg_on_return = ThreadState().transfer;
    return SCE_OK;
}

static int32_t SwitchFiber(FiberObject* object, void* addr_context, uint64_t size_context, uint64_t arg_on_run, uint64_t* arg_on_run_out) {
    auto* target = AsFiber(object);
    if (!target) return object ? SCE_FIBER_ERROR_INVALID : SCE_FIBER_ERROR_NULL;
    auto* self = ThreadState().current;
    if (!self) return SCE_FIBER_ERROR_PERMISSION;
    if (target == self) return SCE_FIBER_ERROR_STATE;
    if (const auto acquired = AcquireWithContext(target, addr_context, size_context); acquired != SCE_OK) return acquired;
    if (TraceFibers()) {
        auto** frame = static_cast<void**>(__builtin_frame_address(0));
        void* chain[6] = {};
        auto** guest = static_cast<void**>(frame[0]);
        for (int depth = 0; depth < 6 && guest; ++depth) {
            chain[depth] = guest[1];
            guest = static_cast<void**>(guest[0]);
        }
        std::fprintf(stderr, "[fiber] switch %s -> %s from %p %p %p %p %p %p\n", self->name, target->name, chain[0], chain[1], chain[2], chain[3], chain[4], chain[5]);
    }
    self->state.store(FiberState::Suspending, std::memory_order_relaxed);
    ThreadState().pendingSuspend = self;
    Resume(target, &self->savedStack, arg_on_run);
    CompletePendingSuspend();
    if (arg_on_run_out) *arg_on_run_out = ThreadState().transfer;
    return SCE_OK;
}

static std::uint64_t CallerFramePointer(void* frame) {
    return reinterpret_cast<std::uint64_t>(static_cast<void**>(frame)[0]);
}

extern "C" {

int32_t APS5_VABI _sceFiberInitializeImpl_nid_postfix(FiberObject* object, const char* name, GuestFiberEntry entry, uint64_t arg_on_initialize, void* addr_context, uint64_t size_context, const void* opt_param, uint32_t build_version) {
    (void)opt_param;
    (void)build_version;
    return InitializeFiber(object, name, entry, arg_on_initialize, addr_context, size_context, 0);
}

int32_t APS5_VABI sceFiberFinalize(FiberObject* object) {
    auto* fiber = AsFiber(object);
    if (!fiber) return object ? SCE_FIBER_ERROR_INVALID : SCE_FIBER_ERROR_NULL;
    const auto state = fiber->state.load(std::memory_order_acquire);
    if (state == FiberState::Running || state == FiberState::Suspending) return SCE_FIBER_ERROR_STATE;
    fiber->magic = 0;
    DetachStack(fiber);
    return SCE_OK;
}

int32_t APS5_VABI sceFiberRun_nid_postfix(FiberObject* object, uint64_t arg_on_run, uint64_t* arg_on_return) {
    return RunFiber(object, nullptr, 0, arg_on_run, arg_on_return, CallerFramePointer(__builtin_frame_address(0)));
}

int32_t APS5_VABI sceFiberSwitch(FiberObject* object, uint64_t arg_on_run, uint64_t* arg_on_run_out) {
    return SwitchFiber(object, nullptr, 0, arg_on_run, arg_on_run_out);
}

int32_t APS5_VABI sceFiberReturnToThread(uint64_t arg_on_return, uint64_t* arg_on_run) {
    auto* self = ThreadState().current;
    if (!self) return SCE_FIBER_ERROR_PERMISSION;
    if (TraceFibers()) std::fprintf(stderr, "[fiber] return %s from %p\n", self->name, __builtin_return_address(0));
    self->state.store(FiberState::Suspending, std::memory_order_relaxed);
    ThreadState().pendingSuspend = self;
    ThreadState().current = nullptr;
    ThreadState().transfer = arg_on_return;
    SetBounds(ThreadState().threadBounds);
    Aps5FiberSwitchStack_nid_no_patch(&self->savedStack, &ThreadState().threadStack, nullptr);
    CompletePendingSuspend();
    if (arg_on_run) *arg_on_run = ThreadState().transfer;
    return SCE_OK;
}

int32_t APS5_VABI sceFiberGetSelf(FiberObject** fiber) {
    if (!fiber) return SCE_FIBER_ERROR_NULL;
    if (!ThreadState().current) return SCE_FIBER_ERROR_PERMISSION;
    *fiber = reinterpret_cast<FiberObject*>(ThreadState().current);
    return SCE_OK;
}

int32_t APS5_VABI sceFiberGetInfo(FiberObject* object, FiberInfo* fiber_info) {
    auto* fiber = AsFiber(object);
    if (!fiber || !fiber_info) return object && fiber_info ? SCE_FIBER_ERROR_INVALID : SCE_FIBER_ERROR_NULL;
    if (fiber_info->size != sizeof(FiberInfo)) return SCE_FIBER_ERROR_INVALID;
    fiber_info->entry = fiber->entry;
    fiber_info->arg_on_initialize = fiber->argOnInitialize;
    fiber_info->addr_context = SizedWindow(fiber);
    fiber_info->size_context = fiber->contextSize;
    std::memcpy(fiber_info->name, fiber->name, sizeof(fiber_info->name));
    fiber_info->size_context_margin = static_cast<uint64_t>(-1);
    if (fiber->contextSizeCheck && fiber->context) {
        const auto* words = SizedWindow(fiber);
        const auto* end = words + fiber->contextSize / sizeof(std::uint64_t);
        fiber_info->size_context_margin = static_cast<uint64_t>(std::find_if(words, end, [](std::uint64_t word) { return word != FIBER_CONTEXT_FILL; }) - words) * sizeof(std::uint64_t);
    }
    return SCE_OK;
}

int32_t APS5_VABI sceFiberRename(FiberObject* object, const char* name) {
    auto* fiber = AsFiber(object);
    if (!fiber || !name) return object && name ? SCE_FIBER_ERROR_INVALID : SCE_FIBER_ERROR_NULL;
    std::memset(fiber->name, 0, sizeof(fiber->name));
    std::strncpy(fiber->name, name, FIBER_MAX_NAME_LENGTH);
    return SCE_OK;
}

int32_t APS5_VABI sceFiberOptParamInitialize(FiberOptParam* opt_param) {
    if (!opt_param) return SCE_FIBER_ERROR_NULL;
    std::memset(opt_param, 0, FIBER_OPT_PARAM_SIZE);
    return SCE_OK;
}

int32_t APS5_VABI sceFiberGetThreadFramePointerAddress(uint64_t* addr_frame_pointer) {
    if (!addr_frame_pointer) return SCE_FIBER_ERROR_NULL;
    if (!ThreadState().current) return SCE_FIBER_ERROR_PERMISSION;
    *addr_frame_pointer = ThreadState().threadFramePointer;
    return SCE_OK;
}

int32_t APS5_VABI _sceFiberGetThreadFramePointerAddress(uint64_t* addr_frame_pointer) {
    return sceFiberGetThreadFramePointerAddress(addr_frame_pointer);
}

int32_t APS5_VABI sceFiberStartContextSizeCheck(uint32_t flags) {
    if (flags != 0) return SCE_FIBER_ERROR_INVALID;
    bool expected = false;
    return g_contextSizeCheck.compare_exchange_strong(expected, true) ? SCE_OK : SCE_FIBER_ERROR_STATE;
}

int32_t APS5_VABI sceFiberStopContextSizeCheck(void) {
    bool expected = true;
    return g_contextSizeCheck.compare_exchange_strong(expected, false) ? SCE_OK : SCE_FIBER_ERROR_STATE;
}

int32_t APS5_VABI _sceFiberAttachContextAndRun(FiberObject* object, void* addr_context, uint64_t size_context, uint64_t arg_on_run, uint64_t* arg_on_return) {
    return RunFiber(object, addr_context, size_context, arg_on_run, arg_on_return, CallerFramePointer(__builtin_frame_address(0)));
}

int32_t APS5_VABI _sceFiberAttachContextAndSwitch(FiberObject* object, void* addr_context, uint64_t size_context, uint64_t arg_on_run, uint64_t* arg_on_run_out) {
    return SwitchFiber(object, addr_context, size_context, arg_on_run, arg_on_run_out);
}

int32_t APS5_VABI _sceFiberInitializeWithInternalOptionImpl(FiberObject* object, const char* name, GuestFiberEntry entry, uint64_t arg_on_initialize, void* addr_context, uint64_t size_context, const FiberOptParam* opt_param, uint32_t flags, uint32_t build_version) {
    (void)opt_param;
    (void)build_version;
    return InitializeFiber(object, name, entry, arg_on_initialize, addr_context, size_context, flags);
}

}
