#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <stdexcept>
#include <string>
#include <xmmintrin.h>

#ifdef _WIN32
#include <windows.h>
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

#ifdef _WIN32

using GuestFiberEntry = void (APS5_VABI*)(std::uint64_t argOnInitialize, std::uint64_t argOnRun);

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
};
static_assert(sizeof(Fiber) <= FIBER_OBJECT_SIZE, "guest reserves 0x100 bytes for SceFiber");

static constexpr std::uint64_t FIBER_MAGIC = 0x5245424946355041ull;

struct StackBounds {
    void* base;
    void* limit;
    void* deallocation;
};

struct ThreadFiberState {
    Fiber* current = nullptr;
    void* threadStack = nullptr;
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
        thread.pendingSuspend->state.store(FiberState::Suspended, std::memory_order_release);
        thread.pendingSuspend = nullptr;
    }
}

extern "C" void Aps5FiberSwitchStack(void** save, void* load);
extern "C" void Aps5FiberTrampoline();

asm(R"(
    .text
    .globl Aps5FiberSwitchStack
    .def Aps5FiberSwitchStack; .scl 2; .type 32; .endef
Aps5FiberSwitchStack:
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
    mov %rdx, %rsp
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

    .globl Aps5FiberTrampoline
    .def Aps5FiberTrampoline; .scl 2; .type 32; .endef
Aps5FiberTrampoline:
    mov %r12, %rcx
    and $-16, %rsp
    sub $32, %rsp
    call Aps5FiberMain
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
static_assert(sizeof(InitialFrame) == 0xa8 + 8 * 8 + 8, "initial fiber frame must match Aps5FiberSwitchStack");

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

static StackBounds FiberBounds(const Fiber* fiber) {
    return {fiber->context + fiber->contextSize, fiber->context, fiber->context};
}

static Fiber* AsFiber(FiberObject* object) {
    auto* fiber = reinterpret_cast<Fiber*>(object);
    return fiber && fiber->magic == FIBER_MAGIC ? fiber : nullptr;
}

extern "C" [[noreturn]] void Aps5FiberMain(Fiber* fiber) {
    CompletePendingSuspend();
    fiber->entry(fiber->argOnInitialize, ThreadState().transfer);
    throw std::runtime_error(std::string("sceFiber: entry function of fiber '") + fiber->name + "' returned");
}

static void PrepareInitialStack(Fiber* fiber) {
    const auto top = reinterpret_cast<std::uintptr_t>(fiber->context + fiber->contextSize) & ~static_cast<std::uintptr_t>(15);
    auto* frame = reinterpret_cast<InitialFrame*>(top - 256);
    std::memset(frame, 0, sizeof(*frame));
    frame->mxcsr = _mm_getcsr();
    std::uint16_t control = 0;
    asm volatile("fnstcw %0" : "=m"(control));
    frame->fpuControl = control;
    frame->r12 = reinterpret_cast<std::uint64_t>(fiber);
    frame->returnAddress = reinterpret_cast<std::uint64_t>(&Aps5FiberTrampoline);
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
            if (state == FiberState::Idle) PrepareInitialStack(target);
            return true;
        }
    }
}

static void Resume(Fiber* target, void** save, std::uint64_t argOnRun) {
    const auto* frame = static_cast<const InitialFrame*>(target->savedStack);
    if (frame->returnAddress == 0) {
        std::fprintf(stderr, "[fiber] resuming '%s' with a wiped context: saved=%p context=%p size=0x%llx\n", target->name, target->savedStack,
                     static_cast<void*>(target->context), static_cast<unsigned long long>(target->contextSize));
        const auto* words = static_cast<const std::uint64_t*>(target->savedStack);
        for (int i = 0; i < 40; i += 4) std::fprintf(stderr, "[fiber]   +0x%03x %016llx %016llx %016llx %016llx\n", i * 8, static_cast<unsigned long long>(words[i]), static_cast<unsigned long long>(words[i + 1]), static_cast<unsigned long long>(words[i + 2]), static_cast<unsigned long long>(words[i + 3]));
        std::fflush(stderr);
        std::abort();
    }
    ThreadState().current = target;
    ThreadState().transfer = argOnRun;
    SetBounds(FiberBounds(target));
    Aps5FiberSwitchStack(save, target->savedStack);
}

extern "C" {

int32_t APS5_VABI _sceFiberInitializeImpl_nid_postfix(FiberObject* object, const char* name, GuestFiberEntry entry, uint64_t arg_on_initialize, void* addr_context, uint64_t size_context, const void* opt_param, uint32_t build_version) {
    (void)opt_param;
    (void)build_version;
    if (!object || !name || !entry) return SCE_FIBER_ERROR_NULL;
    if ((reinterpret_cast<std::uintptr_t>(object) & 7) != 0) return SCE_FIBER_ERROR_ALIGNMENT;
    if (addr_context == nullptr && size_context != 0) return SCE_FIBER_ERROR_INVALID;
    if ((reinterpret_cast<std::uintptr_t>(addr_context) & 15) != 0 || (size_context & 15) != 0) return SCE_FIBER_ERROR_ALIGNMENT;
    if (addr_context == nullptr) return SCE_FIBER_ERROR_INVALID;
    if (size_context < FIBER_MIN_CONTEXT_SIZE) return SCE_FIBER_ERROR_RANGE;
    auto* fiber = reinterpret_cast<Fiber*>(object);
    std::memset(object, 0, FIBER_OBJECT_SIZE);
    fiber->magic = FIBER_MAGIC;
    fiber->state.store(FiberState::Idle, std::memory_order_relaxed);
    fiber->entry = entry;
    fiber->argOnInitialize = arg_on_initialize;
    fiber->context = static_cast<std::uint8_t*>(addr_context);
    fiber->contextSize = size_context;
    std::strncpy(fiber->name, name, FIBER_MAX_NAME_LENGTH);
    if (TraceFibers()) std::fprintf(stderr, "[fiber] init %s object=%p context=%p+0x%llx entry=%p\n", fiber->name, static_cast<void*>(object), addr_context, static_cast<unsigned long long>(size_context), reinterpret_cast<void*>(entry));
    return SCE_OK;
}

int32_t APS5_VABI sceFiberFinalize(FiberObject* object) {
    auto* fiber = AsFiber(object);
    if (!fiber) return object ? SCE_FIBER_ERROR_INVALID : SCE_FIBER_ERROR_NULL;
    const auto state = fiber->state.load(std::memory_order_acquire);
    if (state == FiberState::Running || state == FiberState::Suspending) return SCE_FIBER_ERROR_STATE;
    fiber->magic = 0;
    return SCE_OK;
}

int32_t APS5_VABI sceFiberRun_nid_postfix(FiberObject* object, uint64_t arg_on_run, uint64_t* arg_on_return) {
    auto* fiber = AsFiber(object);
    if (!fiber) return object ? SCE_FIBER_ERROR_INVALID : SCE_FIBER_ERROR_NULL;
    if (ThreadState().current) return SCE_FIBER_ERROR_PERMISSION;
    if (!AcquireForResume(fiber)) return SCE_FIBER_ERROR_STATE;
    ThreadState().threadBounds = CurrentBounds();
    Resume(fiber, &ThreadState().threadStack, arg_on_run);
    CompletePendingSuspend();
    SetBounds(ThreadState().threadBounds);
    if (arg_on_return) *arg_on_return = ThreadState().transfer;
    return SCE_OK;
}

int32_t APS5_VABI sceFiberSwitch(FiberObject* object, uint64_t arg_on_run, uint64_t* arg_on_run_out) {
    auto* target = AsFiber(object);
    if (!target) return object ? SCE_FIBER_ERROR_INVALID : SCE_FIBER_ERROR_NULL;
    auto* self = ThreadState().current;
    if (!self) return SCE_FIBER_ERROR_PERMISSION;
    if (target == self || !AcquireForResume(target)) return SCE_FIBER_ERROR_STATE;
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

int32_t APS5_VABI sceFiberReturnToThread(uint64_t arg_on_return, uint64_t* arg_on_run) {
    auto* self = ThreadState().current;
    if (!self) return SCE_FIBER_ERROR_PERMISSION;
    if (TraceFibers()) std::fprintf(stderr, "[fiber] return %s from %p\n", self->name, __builtin_return_address(0));
    self->state.store(FiberState::Suspending, std::memory_order_relaxed);
    ThreadState().pendingSuspend = self;
    ThreadState().current = nullptr;
    ThreadState().transfer = arg_on_return;
    SetBounds(ThreadState().threadBounds);
    Aps5FiberSwitchStack(&self->savedStack, ThreadState().threadStack);
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
    fiber_info->entry = reinterpret_cast<FiberEntry>(fiber->entry);
    fiber_info->arg_on_initialize = fiber->argOnInitialize;
    fiber_info->addr_context = fiber->context;
    fiber_info->size_context = fiber->contextSize;
    std::memcpy(fiber_info->name, fiber->name, sizeof(fiber_info->name));
    fiber_info->size_context_margin = static_cast<uint64_t>(-1);
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
    (void)addr_frame_pointer;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceFiberStartContextSizeCheck(uint32_t flags) {
    (void)flags;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceFiberStopContextSizeCheck(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

#else

extern "C" {

int32_t APS5_VABI _sceFiberInitializeImpl_nid_postfix() { NotImplemented_nid_no_patch(__func__); return 0; }
int32_t APS5_VABI sceFiberFinalize(FiberObject*) { NotImplemented_nid_no_patch(__func__); return 0; }
int32_t APS5_VABI sceFiberRun_nid_postfix(FiberObject*, uint64_t, uint64_t*) { NotImplemented_nid_no_patch(__func__); return 0; }
int32_t APS5_VABI sceFiberSwitch(FiberObject*, uint64_t, uint64_t*) { NotImplemented_nid_no_patch(__func__); return 0; }
int32_t APS5_VABI sceFiberReturnToThread(uint64_t, uint64_t*) { NotImplemented_nid_no_patch(__func__); return 0; }
int32_t APS5_VABI sceFiberGetSelf(FiberObject**) { NotImplemented_nid_no_patch(__func__); return 0; }
int32_t APS5_VABI sceFiberGetInfo(FiberObject*, FiberInfo*) { NotImplemented_nid_no_patch(__func__); return 0; }
int32_t APS5_VABI sceFiberRename(FiberObject*, const char*) { NotImplemented_nid_no_patch(__func__); return 0; }
int32_t APS5_VABI sceFiberOptParamInitialize(FiberOptParam*) { NotImplemented_nid_no_patch(__func__); return 0; }
int32_t APS5_VABI sceFiberGetThreadFramePointerAddress(uint64_t*) { NotImplemented_nid_no_patch(__func__); return 0; }
int32_t APS5_VABI sceFiberStartContextSizeCheck(uint32_t) { NotImplemented_nid_no_patch(__func__); return 0; }
int32_t APS5_VABI sceFiberStopContextSizeCheck(void) { NotImplemented_nid_no_patch(__func__); return 0; }

}

#endif
