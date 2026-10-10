#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <array>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" Pthread APS5_VABI scePthreadSelf();
#ifdef _WIN32
extern "C" void Aps5RedirectedEntryStub();
extern "C" [[noreturn]] void Aps5RedirectedExit(CONTEXT* context, const void* vectors, void (*restore)(CONTEXT*, EXCEPTION_RECORD*));
#endif

namespace {

struct GuestMcontext {
    std::uint64_t onstack;
    std::uint64_t rdi;
    std::uint64_t rsi;
    std::uint64_t rdx;
    std::uint64_t rcx;
    std::uint64_t r8;
    std::uint64_t r9;
    std::uint64_t rax;
    std::uint64_t rbx;
    std::uint64_t rbp;
    std::uint64_t r10;
    std::uint64_t r11;
    std::uint64_t r12;
    std::uint64_t r13;
    std::uint64_t r14;
    std::uint64_t r15;
    std::uint32_t trapno;
    std::uint16_t fs;
    std::uint16_t gs;
    std::uint64_t addr;
    std::uint32_t flags;
    std::uint16_t es;
    std::uint16_t ds;
    std::uint64_t err;
    std::uint64_t rip;
    std::uint64_t cs;
    std::uint64_t rflags;
    std::uint64_t rsp;
    std::uint64_t ss;
    std::uint64_t len;
    std::uint64_t fpformat;
    std::uint64_t ownedfp;
    std::uint64_t lbrfrom;
    std::uint64_t lbrto;
    std::uint64_t aux1;
    std::uint64_t aux2;
    std::uint64_t fpstate[104];
    std::uint64_t fsbase;
    std::uint64_t gsbase;
    std::uint64_t spare[6];
};

struct GuestUcontext {
    std::uint32_t sigmask[4];
    std::int32_t reserved[12];
    GuestMcontext mcontext;
    GuestUcontext* link;
    void* stackPointer;
    std::uint64_t stackSize;
    std::int32_t stackFlags;
    std::int32_t stackAlign;
    std::int32_t flags;
    std::int32_t spare[4];
    std::int32_t tail[3];
};

static_assert(offsetof(GuestUcontext, mcontext) == 0x40);
static_assert(offsetof(GuestUcontext, mcontext) + offsetof(GuestMcontext, rsp) == 0xf8);

using GuestExceptionHandler = void (APS5_VABI *)(int, void*);

constexpr std::array<int, 6> AllowedSignals{1, 4, 8, 10, 11, 30};

std::mutex handlersLock;
std::array<void*, 32> handlers{};

bool Allowed(int signum) {
    for (const int allowed : AllowedSignals)
        if (allowed == signum) return true;
    return false;
}

GuestExceptionHandler Handler(int signum) {
    std::lock_guard lock(handlersLock);
    return reinterpret_cast<GuestExceptionHandler>(handlers[signum]);
}

#ifdef _WIN32
constexpr std::size_t RedZone = 128;
constexpr std::size_t HomeArea = 32;
constexpr auto RetryLimit = std::chrono::seconds(1);
constexpr std::size_t VectorBytes = 16 * 32;

struct Delivery {
    GuestExceptionHandler handler;
    int signum;
    CONTEXT context;
    std::uint8_t vectors[VectorBytes];
    std::uint64_t saveVectors;
};

const bool SaveVectors = [] {
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx") != 0;
}();

void Deliver(GuestExceptionHandler handler, int signum, CONTEXT& context) {
    GuestUcontext ucontext{};
    auto& m = ucontext.mcontext;
    m.rdi = context.Rdi;
    m.rsi = context.Rsi;
    m.rdx = context.Rdx;
    m.rcx = context.Rcx;
    m.r8 = context.R8;
    m.r9 = context.R9;
    m.rax = context.Rax;
    m.rbx = context.Rbx;
    m.rbp = context.Rbp;
    m.r10 = context.R10;
    m.r11 = context.R11;
    m.r12 = context.R12;
    m.r13 = context.R13;
    m.r14 = context.R14;
    m.r15 = context.R15;
    m.rip = context.Rip;
    m.rsp = context.Rsp;
    m.rflags = context.EFlags;
    m.cs = context.SegCs;
    m.ss = context.SegSs;
    m.len = sizeof(GuestMcontext);
    static_assert(sizeof(context.FltSave) <= sizeof(m.fpstate));
    std::memcpy(m.fpstate, &context.FltSave, sizeof(context.FltSave));
    handler(signum, &ucontext);
    context.Rdi = m.rdi;
    context.Rsi = m.rsi;
    context.Rdx = m.rdx;
    context.Rcx = m.rcx;
    context.R8 = m.r8;
    context.R9 = m.r9;
    context.Rax = m.rax;
    context.Rbx = m.rbx;
    context.Rbp = m.rbp;
    context.R10 = m.r10;
    context.R11 = m.r11;
    context.R12 = m.r12;
    context.R13 = m.r13;
    context.R14 = m.r14;
    context.R15 = m.r15;
    context.Rip = m.rip;
    context.Rsp = m.rsp;
    context.EFlags = static_cast<DWORD>(m.rflags);
    std::memcpy(&context.FltSave, m.fpstate, sizeof(context.FltSave));
}

[[noreturn]] void RedirectedEntry(Delivery* delivery) {
    CONTEXT context = delivery->context;
    Deliver(delivery->handler, delivery->signum, context);
    Aps5RedirectedExit(&context, delivery->saveVectors != 0 ? delivery->vectors : nullptr, RtlRestoreContext);
}

bool StackWritable(DWORD64 low, DWORD64 high) {
    for (DWORD64 address = low; address < high;) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) != sizeof(info)) return false;
        if (info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 || (info.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) == 0) return false;
        address = reinterpret_cast<DWORD64>(info.BaseAddress) + info.RegionSize;
    }
    return true;
}

void CALLBACK WaitingEntry(ULONG_PTR parameter) {
    auto* delivery = reinterpret_cast<Delivery*>(parameter);
    const auto handler = delivery->handler;
    const int signum = delivery->signum;
    delete delivery;
    CONTEXT context{};
    RtlCaptureContext(&context);
    Deliver(handler, signum, context);
}

static_assert(HomeArea + 8 == 40, "Aps5RedirectedEntryStub finds the delivery 40 bytes above its stack pointer");
static_assert(offsetof(Delivery, context) == 16 && offsetof(CONTEXT, Rax) == 0x78 && offsetof(CONTEXT, Rbp) == 0xa0 && offsetof(CONTEXT, R15) == 0xf0, "Aps5RedirectedEntryStub stores the live registers into the delivery's context");
static_assert(offsetof(Delivery, vectors) == 1248 && offsetof(Delivery, saveVectors) == 1760, "Aps5RedirectedEntryStub stores ymm0-ymm15 into the delivery when saveVectors is set");

bool Exited(HANDLE native) {
    return WaitForSingleObject(native, 0) == WAIT_OBJECT_0;
}

bool RestoringContext(DWORD64 rip) {
    static const std::array<DWORD64, 2> stubs = [] {
        const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        return std::array<DWORD64, 2>{reinterpret_cast<DWORD64>(GetProcAddress(ntdll, "NtContinue")), reinterpret_cast<DWORD64>(GetProcAddress(ntdll, "NtContinueEx"))};
    }();
    for (const DWORD64 stub : stubs)
        if (stub != 0 && rip - stub < 0x20) return true;
    return false;
}

bool InWinpthread(DWORD64 rip) {
    static const HMODULE winpthread = GetModuleHandleW(L"libwinpthread-1.dll");
    MEMORY_BASIC_INFORMATION info{};
    return winpthread != nullptr && VirtualQuery(reinterpret_cast<const void*>(rip), &info, sizeof(info)) == sizeof(info) && info.AllocationBase == winpthread;
}

bool RaiseOn(Pthread thread, GuestExceptionHandler handler, int signum) {
    if (thread == scePthreadSelf()) {
        CONTEXT context{};
        RtlCaptureContext(&context);
        Deliver(handler, signum, context);
        return true;
    }
    const auto native = static_cast<HANDLE>(thread->nativeHandle);
    auto queued = std::make_unique<Delivery>(Delivery{handler, signum, {}, {}, 0});
    alignas(16) Delivery delivery{handler, signum, {}, {}, SaveVectors ? 1u : 0u};
    const auto retryDeadline = std::chrono::steady_clock::now() + RetryLimit;
    for (;;) {
        if (SuspendThread(native) == static_cast<DWORD>(-1)) {
            if (Exited(native)) return false;
            throw std::runtime_error("sceKernelRaiseException: cannot suspend the target thread");
        }
        if (Exited(native)) {
            ResumeThread(native);
            return false;
        }
        delivery.context.ContextFlags = CONTEXT_FULL | CONTEXT_SEGMENTS;
        if (!GetThreadContext(native, &delivery.context)) {
            ResumeThread(native);
            throw std::runtime_error("sceKernelRaiseException: cannot read the target thread context");
        }
        if (thread->waitCount.load(std::memory_order_seq_cst) > 0) {
            const bool accepted = QueueUserAPC(WaitingEntry, native, reinterpret_cast<ULONG_PTR>(queued.get())) != 0;
            ResumeThread(native);
            if (!accepted) throw std::runtime_error("sceKernelRaiseException: cannot queue delivery to the waiting thread");
            queued.release();
            return true;
        }
        if (!RestoringContext(delivery.context.Rip) && !InWinpthread(delivery.context.Rip)) break;
        ResumeThread(native);
        if (std::chrono::steady_clock::now() >= retryDeadline) throw std::runtime_error("sceKernelRaiseException: the target thread stayed inside NtContinue or winpthreads for 1 s");
        SwitchToThread();
    }
    const DWORD64 slot = (delivery.context.Rsp - RedZone - sizeof(Delivery)) & ~static_cast<DWORD64>(15);
    if (!StackWritable(slot - HomeArea - 8, delivery.context.Rsp - RedZone)) {
        ResumeThread(native);
        throw std::runtime_error("sceKernelRaiseException: the target thread stack below its red zone is not committed");
    }
    std::memcpy(reinterpret_cast<void*>(slot), &delivery, sizeof(Delivery));
    CONTEXT redirected = delivery.context;
    redirected.Rsp = slot - HomeArea - 8;
    redirected.Rip = reinterpret_cast<DWORD64>(&Aps5RedirectedEntryStub);
    if (!SetThreadContext(native, &redirected)) {
        ResumeThread(native);
        throw std::runtime_error("sceKernelRaiseException: cannot redirect the target thread");
    }
    ResumeThread(native);
    return true;
}
#endif

}

#ifdef _WIN32
extern "C" [[noreturn]] void Aps5RedirectedEntry(void* delivery) {
    RedirectedEntry(static_cast<Delivery*>(delivery));
}
asm(".text\n"
    ".globl Aps5RedirectedEntryStub\n"
    "Aps5RedirectedEntryStub:\n"
    "    movq %rax, 176(%rsp)\n"
    "    movq %rcx, 184(%rsp)\n"
    "    movq %rdx, 192(%rsp)\n"
    "    movq %rbx, 200(%rsp)\n"
    "    movq %rbp, 216(%rsp)\n"
    "    movq %rsi, 224(%rsp)\n"
    "    movq %rdi, 232(%rsp)\n"
    "    movq %r8, 240(%rsp)\n"
    "    movq %r9, 248(%rsp)\n"
    "    movq %r10, 256(%rsp)\n"
    "    movq %r11, 264(%rsp)\n"
    "    movq %r12, 272(%rsp)\n"
    "    movq %r13, 280(%rsp)\n"
    "    movq %r14, 288(%rsp)\n"
    "    movq %r15, 296(%rsp)\n"
    "    cmpq $0, 1800(%rsp)\n"
    "    je 1f\n"
    "    vmovdqu %ymm0, 1288(%rsp)\n"
    "    vmovdqu %ymm1, 1320(%rsp)\n"
    "    vmovdqu %ymm2, 1352(%rsp)\n"
    "    vmovdqu %ymm3, 1384(%rsp)\n"
    "    vmovdqu %ymm4, 1416(%rsp)\n"
    "    vmovdqu %ymm5, 1448(%rsp)\n"
    "    vmovdqu %ymm6, 1480(%rsp)\n"
    "    vmovdqu %ymm7, 1512(%rsp)\n"
    "    vmovdqu %ymm8, 1544(%rsp)\n"
    "    vmovdqu %ymm9, 1576(%rsp)\n"
    "    vmovdqu %ymm10, 1608(%rsp)\n"
    "    vmovdqu %ymm11, 1640(%rsp)\n"
    "    vmovdqu %ymm12, 1672(%rsp)\n"
    "    vmovdqu %ymm13, 1704(%rsp)\n"
    "    vmovdqu %ymm14, 1736(%rsp)\n"
    "    vmovdqu %ymm15, 1768(%rsp)\n"
    "1:\n"
    "    leaq 40(%rsp), %rcx\n"
    "    jmp Aps5RedirectedEntry\n"
    ".globl Aps5RedirectedExit\n"
    "Aps5RedirectedExit:\n"
    "    testq %rdx, %rdx\n"
    "    je 1f\n"
    "    vmovdqu 0(%rdx), %ymm0\n"
    "    vmovdqu 32(%rdx), %ymm1\n"
    "    vmovdqu 64(%rdx), %ymm2\n"
    "    vmovdqu 96(%rdx), %ymm3\n"
    "    vmovdqu 128(%rdx), %ymm4\n"
    "    vmovdqu 160(%rdx), %ymm5\n"
    "    vmovdqu 192(%rdx), %ymm6\n"
    "    vmovdqu 224(%rdx), %ymm7\n"
    "    vmovdqu 256(%rdx), %ymm8\n"
    "    vmovdqu 288(%rdx), %ymm9\n"
    "    vmovdqu 320(%rdx), %ymm10\n"
    "    vmovdqu 352(%rdx), %ymm11\n"
    "    vmovdqu 384(%rdx), %ymm12\n"
    "    vmovdqu 416(%rdx), %ymm13\n"
    "    vmovdqu 448(%rdx), %ymm14\n"
    "    vmovdqu 480(%rdx), %ymm15\n"
    "1:\n"
    "    xorl %edx, %edx\n"
    "    jmp *%r8\n");
#endif

extern "C" {

int APS5_VABI sceKernelInstallExceptionHandler(int signum, void* handler) {
 if (!Allowed(signum) || handler == nullptr) return SCE_KERNEL_ERROR_EINVAL;
 std::lock_guard lock(handlersLock);
 if (handlers[signum] != nullptr) return SCE_KERNEL_ERROR_EAGAIN;
 handlers[signum] = handler;
 return 0;
}

int APS5_VABI sceKernelRemoveExceptionHandler(int signum) {
 if (!Allowed(signum)) return SCE_KERNEL_ERROR_EINVAL;
 std::lock_guard lock(handlersLock);
 handlers[signum] = nullptr;
 return 0;
}

int APS5_VABI sceKernelRaiseException(Pthread thread, int signum) {
 if (signum != 30) return SCE_KERNEL_ERROR_EINVAL;
 if (thread == nullptr || thread->_finished.load(std::memory_order_acquire)) return SCE_KERNEL_ERROR_ESRCH;
 const auto handler = Handler(signum);
 if (handler == nullptr) throw std::runtime_error("sceKernelRaiseException: no handler installed for the signal");
#ifdef _WIN32
 return RaiseOn(thread, handler, signum) ? 0 : SCE_KERNEL_ERROR_ESRCH;
#else
 NotImplemented_nid_no_patch(__func__);
 return 0;
#endif
}

void APS5_VABI sceKernelDebugRaiseException(int c1, int c2) {
  APS5_LOG_OUT("sceKernelDebugRaiseException c1=%d c2=%d", c1, c2);
}

void APS5_VABI sceKernelDebugRaiseExceptionOnReleaseMode(int c1, int c2) {
  APS5_LOG_OUT("sceKernelDebugRaiseExceptionOnReleaseMode c1=%d c2=%d", c1, c2);
}

}
