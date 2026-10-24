#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/HostFault.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <vector>
#include <algorithm>
#include <array>
#include <chrono>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#ifdef _WIN32
#include <string_view>
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <system_error>
#include <pthread.h>
#include <ucontext.h>
#include <unistd.h>
#endif
#ifdef __linux__
#include <string_view>
#include <dlfcn.h>
#include <link.h>
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35))
#define LIBKERNEL_EXCEPTION_FIND_OBJECT 1
#endif
#endif

extern "C" Pthread APS5_VABI scePthreadSelf();
#ifdef _WIN32
extern "C" void Aps5RedirectedEntryStub();
extern "C" void Aps5FaultEntryStub();
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

struct alignas(16) GuestUcontext {
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
constexpr int RaisedSignal = 30;
constexpr int SignalIllegal = 4;
constexpr int SignalFloatingPoint = 8;
constexpr int SignalBus = 10;
constexpr int SignalSegmentation = 11;

constexpr std::uint32_t TrapPrivilegedInstruction = 1;
constexpr std::uint32_t TrapArithmetic = 6;
constexpr std::uint32_t TrapProtection = 9;
constexpr std::uint32_t TrapPage = 12;
constexpr std::uint32_t TrapAlignment = 14;
constexpr std::uint32_t TrapDivide = 18;
constexpr std::uint32_t TrapSegmentNotPresent = 26;
constexpr std::uint32_t TrapStack = 27;
constexpr std::uint32_t TrapSimd = 29;

constexpr std::uint64_t PageProtection = 0x1;
constexpr std::uint64_t PageWrite = 0x2;
constexpr std::uint64_t PageUser = 0x4;
constexpr std::uint64_t PageFetch = 0x10;

struct Trap {
    std::uint32_t number;
    std::uint64_t address;
    std::uint64_t error;
};

struct Fault {
    int signum;
    Trap trap;
};

std::array<std::atomic<void*>, 32> handlers{};
static_assert(std::atomic<void*>::is_always_lock_free);

bool Allowed(int signum) {
    for (const int allowed : AllowedSignals)
        if (allowed == signum) return true;
    return false;
}

GuestExceptionHandler Handler(int signum) {
    return reinterpret_cast<GuestExceptionHandler>(handlers[signum].load(std::memory_order_acquire));
}

constexpr std::size_t MxcsrOffset = 24;
constexpr std::size_t MxcsrMaskOffset = 28;
constexpr std::uint32_t DefaultMxcsrMask = 0xffbf;

std::uint32_t FpWord(const GuestMcontext& m, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, reinterpret_cast<const unsigned char*>(m.fpstate) + offset, sizeof(value));
    return value;
}

void SetFpWord(GuestMcontext& m, std::size_t offset, std::uint32_t value) {
    std::memcpy(reinterpret_cast<unsigned char*>(m.fpstate) + offset, &value, sizeof(value));
}

char* AppendText(char* output, const char* text) {
    while (*text != '\0') *output++ = *text++;
    return output;
}

char* AppendHex(char* output, std::uint64_t value, std::size_t digits) {
    static constexpr char hexDigits[] = "0123456789abcdef";
    while (digits != 0) {
        --digits;
        *output++ = hexDigits[(value >> (digits * 4)) & 0xf];
    }
    return output;
}

char* AppendHexField(char* output, const char* name, std::uint64_t value, std::size_t digits) {
    return AppendHex(AppendText(output, name), value, digits);
}

void LogGuestExceptionContext(int signum, const GuestMcontext& context) {
    char buffer[768];
    char* output = AppendText(buffer, "[exception] guest context");
    output = AppendHexField(output, " signal=0x", static_cast<std::uint32_t>(signum), 2);
    output = AppendHexField(output, " rip=0x", context.rip, 16);
    output = AppendHexField(output, " rsp=0x", context.rsp, 16);
    output = AppendHexField(output, " rflags=0x", context.rflags, 16);
    output = AppendHexField(output, " trap=0x", context.trapno, 8);
    output = AppendHexField(output, " error=0x", context.err, 16);
    output = AppendHexField(output, " fault=0x", context.addr, 16);
    output = AppendHexField(output, " rax=0x", context.rax, 16);
    output = AppendHexField(output, " rbx=0x", context.rbx, 16);
    output = AppendHexField(output, " rcx=0x", context.rcx, 16);
    output = AppendHexField(output, " rdx=0x", context.rdx, 16);
    output = AppendHexField(output, " rsi=0x", context.rsi, 16);
    output = AppendHexField(output, " rdi=0x", context.rdi, 16);
    output = AppendHexField(output, " rbp=0x", context.rbp, 16);
    output = AppendHexField(output, " r8=0x", context.r8, 16);
    output = AppendHexField(output, " r9=0x", context.r9, 16);
    output = AppendHexField(output, " r10=0x", context.r10, 16);
    output = AppendHexField(output, " r11=0x", context.r11, 16);
    output = AppendHexField(output, " r12=0x", context.r12, 16);
    output = AppendHexField(output, " r13=0x", context.r13, 16);
    output = AppendHexField(output, " r14=0x", context.r14, 16);
    output = AppendHexField(output, " r15=0x", context.r15, 16);
    *output++ = '\n';
#ifdef _WIN32
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), buffer, static_cast<DWORD>(output - buffer), &written, nullptr);
#else
    const auto written = write(STDERR_FILENO, buffer, static_cast<std::size_t>(output - buffer));
    (void)written;
#endif
}

bool FaultSignal(int signum) {
    return signum == SignalIllegal || signum == SignalFloatingPoint || signum == SignalBus || signum == SignalSegmentation;
}

void CallHandler(GuestExceptionHandler handler, int signum, GuestUcontext& ucontext, const Trap& trap = {}) {
    auto& m = ucontext.mcontext;
    m.trapno = trap.number;
    m.addr = trap.address;
    m.err = trap.error;
    const std::uint32_t mxcsrMask = FpWord(m, MxcsrMaskOffset);
    LogGuestExceptionContext(signum, m);
    handler(signum, &ucontext);
    SetFpWord(m, MxcsrOffset, FpWord(m, MxcsrOffset) & (mxcsrMask != 0 ? mxcsrMask : DefaultMxcsrMask));
    SetFpWord(m, MxcsrMaskOffset, mxcsrMask);
}

#ifdef _WIN32
constexpr std::size_t RedZone = 128;
constexpr std::size_t HomeArea = 32;
constexpr auto RetryLimit = std::chrono::seconds(1);
constexpr auto HostCodeLimit = std::chrono::milliseconds(50);
constexpr auto HostCodeResample = std::chrono::microseconds(20);
constexpr std::size_t VectorBytes = 16 * 32;

struct Delivery {
    GuestExceptionHandler handler;
    int signum;
    CONTEXT context;
    std::uint8_t vectors[VectorBytes];
    std::uint64_t saveVectors;
    Trap trap;
};

const bool SaveVectors = [] {
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx") != 0;
}();

void Deliver(GuestExceptionHandler handler, int signum, CONTEXT& context, const Trap& trap) {
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
    CallHandler(handler, signum, ucontext, trap);
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
    context.MxCsr = context.FltSave.MxCsr;
}

[[noreturn]] void RedirectedEntry(Delivery* delivery) {
    CONTEXT context = delivery->context;
    Deliver(delivery->handler, delivery->signum, context, delivery->trap);
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
    Deliver(handler, signum, context, {});
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

enum class CodeKind : std::uint8_t { Unknown, Guest, Host };

struct ImageKinds {
    static constexpr std::size_t Capacity = 256;
    std::array<std::atomic<DWORD64>, Capacity> bases{};
    std::array<std::atomic<std::uint8_t>, Capacity> kinds{};
    std::atomic<std::size_t> count{0};
};

ImageKinds& Images() {
    static ImageKinds images;
    return images;
}

DWORD64 ImageBase(DWORD64 rip) {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(rip), &info, sizeof(info)) != sizeof(info) || info.Type != MEM_IMAGE) return 0;
    return reinterpret_cast<DWORD64>(info.AllocationBase);
}

CodeKind CachedKind(DWORD64 base) {
    if (base == 0) return CodeKind::Guest;
    auto& images = Images();
    const auto count = std::min(images.count.load(std::memory_order_acquire), ImageKinds::Capacity);
    for (std::size_t index = 0; index < count; ++index) {
        if (images.bases[index].load(std::memory_order_acquire) == base) return static_cast<CodeKind>(images.kinds[index].load(std::memory_order_acquire));
    }
    return CodeKind::Unknown;
}

DWORD64 ExecutableBase() {
    static const auto executable = reinterpret_cast<DWORD64>(GetModuleHandleW(nullptr));
    return executable;
}

CodeKind ClassifyImage(DWORD64 base) {
    if (const auto known = CachedKind(base); known != CodeKind::Unknown) return known;
    auto kind = CodeKind::Host;
    if (base == ExecutableBase()) {
        kind = CodeKind::Guest;
    } else {
        wchar_t path[MAX_PATH]{};
        const auto length = GetModuleFileNameW(reinterpret_cast<HMODULE>(base), path, MAX_PATH);
        const std::wstring_view name(path, length);
        constexpr std::wstring_view guestSuffix = L".guest.prx";
        if (name.size() >= guestSuffix.size() && name.substr(name.size() - guestSuffix.size()) == guestSuffix) kind = CodeKind::Guest;
    }
    auto& images = Images();
    if (const auto slot = images.count.fetch_add(1, std::memory_order_acq_rel); slot < ImageKinds::Capacity) {
        images.kinds[slot].store(static_cast<std::uint8_t>(kind), std::memory_order_release);
        images.bases[slot].store(base, std::memory_order_release);
    }
    return kind;
}

bool BlockedInWait(DWORD64 rip) {
    static const std::vector<DWORD64> waits = [] {
        std::vector<DWORD64> stubs;
        const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        for (const char* name : {"NtWaitForSingleObject", "NtWaitForMultipleObjects", "NtWaitForAlertByThreadId", "NtDelayExecution", "NtRemoveIoCompletion", "NtRemoveIoCompletionEx", "NtWaitForKeyedEvent", "NtSignalAndWaitForSingleObject", "NtWaitForWorkViaWorkerFactory"}) {
            if (const auto address = reinterpret_cast<DWORD64>(GetProcAddress(ntdll, name)); address != 0) stubs.push_back(address);
        }
        if (const HMODULE win32u = GetModuleHandleW(L"win32u.dll"); win32u != nullptr) {
            if (const auto address = reinterpret_cast<DWORD64>(GetProcAddress(win32u, "NtUserMsgWaitForMultipleObjectsEx")); address != 0) stubs.push_back(address);
        }
        return stubs;
    }();
    return std::any_of(waits.begin(), waits.end(), [rip](DWORD64 stub) { return rip - stub < 0x20; });
}

DWORD64 NtdllBase() {
    static const auto ntdll = reinterpret_cast<DWORD64>(GetModuleHandleW(L"ntdll.dll"));
    return ntdll;
}

bool InSystemCall(DWORD64 rip, DWORD64 base) {
    if (base == 0 || base != NtdllBase() || rip < base + 2) return false;
    const auto* code = reinterpret_cast<const unsigned char*>(rip - 2);
    return code[0] == 0x0f && code[1] == 0x05;
}

CodeKind RunningCode(DWORD64 rip, DWORD64& base) {
    base = ImageBase(rip);
    auto kind = CachedKind(base);
    if (kind == CodeKind::Unknown && Images().count.load(std::memory_order_acquire) >= ImageKinds::Capacity) kind = CodeKind::Host;
    return kind == CodeKind::Host && BlockedInWait(rip) ? CodeKind::Guest : kind;
}

bool RaiseOn(Pthread thread, GuestExceptionHandler handler, int signum) {
    if (thread == scePthreadSelf()) {
        CONTEXT context{};
        RtlCaptureContext(&context);
        Deliver(handler, signum, context, {});
        return true;
    }
    const auto native = static_cast<HANDLE>(thread->nativeHandle);
    auto queued = std::make_unique<Delivery>(Delivery{handler, signum, {}, {}, 0, {}});
    alignas(16) Delivery delivery{handler, signum, {}, {}, SaveVectors ? 1u : 0u, {}};
    const auto start = std::chrono::steady_clock::now();
    const auto retryDeadline = start + RetryLimit;
    auto hostDeadline = start + HostCodeLimit;
    static_cast<void>(RestoringContext(0));
    static_cast<void>(InWinpthread(0));
    static_cast<void>(BlockedInWait(0));
    static_cast<void>(ExecutableBase());
    static_cast<void>(Images());
    static_cast<void>(NtdllBase());
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
            if (std::chrono::steady_clock::now() >= retryDeadline) throw std::runtime_error("sceKernelRaiseException: cannot read the target thread context");
            SwitchToThread();
            continue;
        }
        if (thread->waitCount.load(std::memory_order_seq_cst) > 0) {
            const bool accepted = QueueUserAPC(WaitingEntry, native, reinterpret_cast<ULONG_PTR>(queued.get())) != 0;
            ResumeThread(native);
            if (!accepted) throw std::runtime_error("sceKernelRaiseException: cannot queue delivery to the waiting thread");
            queued.release();
            return true;
        }
        const bool restoring = RestoringContext(delivery.context.Rip) || InWinpthread(delivery.context.Rip);
        DWORD64 image = 0;
        const auto running = restoring ? CodeKind::Guest : RunningCode(delivery.context.Rip, image);
        const auto now = std::chrono::steady_clock::now();
        if (running == CodeKind::Host && InSystemCall(delivery.context.Rip, image) && now < retryDeadline) hostDeadline = std::max(hostDeadline, std::min(retryDeadline, now + HostCodeLimit));
        if (!restoring && (running == CodeKind::Guest || (running == CodeKind::Host && now >= hostDeadline))) break;
        ResumeThread(native);
        if (running == CodeKind::Unknown) {
            static_cast<void>(ClassifyImage(image));
            continue;
        }
        if (restoring && std::chrono::steady_clock::now() >= retryDeadline) throw std::runtime_error("sceKernelRaiseException: the target thread stayed inside NtContinue or winpthreads for 1 s");
        if (running == CodeKind::Host) {
            const auto resample = std::chrono::steady_clock::now() + HostCodeResample;
            while (std::chrono::steady_clock::now() < resample) YieldProcessor();
        } else {
            SwitchToThread();
        }
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

constexpr DWORD FloatMultipleFaults = 0xc00002b4;
constexpr DWORD FloatMultipleTraps = 0xc00002b5;
constexpr ULONG_PTR AccessExecute = 8;
constexpr std::uint32_t MxcsrFlags = 0x3f;
constexpr int MxcsrMaskShift = 7;

struct PendingFault {
    GuestExceptionHandler handler;
    int signum;
    Trap trap;
    CONTEXT context;
};

thread_local PendingFault pendingFault;

bool Committed(ULONG_PTR address) {
    MEMORY_BASIC_INFORMATION info{};
    return VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) == sizeof(info) && info.State == MEM_COMMIT && (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
}

std::uint32_t FloatingPointTrap(const CONTEXT& context) {
    const std::uint32_t mxcsr = context.MxCsr;
    return (mxcsr & MxcsrFlags & ~(mxcsr >> MxcsrMaskShift)) != 0 ? TrapSimd : TrapArithmetic;
}

bool Classify(const EXCEPTION_RECORD& record, const CONTEXT& context, Fault& fault) {
    switch (record.ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION: {
        if (record.NumberParameters < 2) return false;
        const ULONG_PTR access = record.ExceptionInformation[0];
        const ULONG_PTR address = record.ExceptionInformation[1];
        if (address == ~static_cast<ULONG_PTR>(0)) {
            fault = {SignalBus, {TrapProtection, 0, 0}};
            return true;
        }
        std::uint64_t error = PageUser;
        if (access == 1) error |= PageWrite;
        if (access == AccessExecute) error |= PageFetch;
        if (Committed(address)) error |= PageProtection;
        fault = {SignalSegmentation, {TrapPage, address, error}};
        return true;
    }
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        fault = {SignalIllegal, {TrapPrivilegedInstruction, 0, 0}};
        return true;
    case EXCEPTION_PRIV_INSTRUCTION:
        fault = {SignalBus, {TrapProtection, 0, 0}};
        return true;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
        fault = {SignalFloatingPoint, {TrapDivide, 0, 0}};
        return true;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        fault = {SignalBus, {TrapAlignment, 0, 0}};
        return true;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
    case FloatMultipleFaults:
    case FloatMultipleTraps:
        fault = {SignalFloatingPoint, {FloatingPointTrap(context), 0, 0}};
        return true;
    default:
        return false;
    }
}

bool GuestCode(DWORD64 rip) {
    if (GuestArena::GuestArenaContains_nid_postfix(reinterpret_cast<const void*>(rip), 1)) return true;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(rip), &info, sizeof(info)) != sizeof(info) || info.Type != MEM_IMAGE) return false;
    const auto* image = static_cast<const unsigned char*>(info.AllocationBase);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    const DWORD64 rva = rip - reinterpret_cast<DWORD64>(image);
    bool relinked = false;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (std::memcmp(sections[i].Name, ".elf", 4) != 0) continue;
        relinked = true;
        if (rva >= sections[i].VirtualAddress && rva < static_cast<DWORD64>(sections[i].VirtualAddress) + sections[i].Misc.VirtualSize) return true;
    }
    return !relinked && info.AllocationBase == GetModuleHandleW(nullptr);
}

bool DeliverHostFault(void* exceptionPointers) noexcept {
    auto& info = *static_cast<EXCEPTION_POINTERS*>(exceptionPointers);
    auto& context = *info.ContextRecord;
    Fault fault{};
    if (!Classify(*info.ExceptionRecord, context, fault)) return false;
    const auto handler = Handler(fault.signum);
    if (handler == nullptr || !GuestCode(context.Rip)) return false;
    auto& pending = pendingFault;
    if (pending.handler != nullptr) return false;
    pending.handler = handler;
    pending.signum = fault.signum;
    pending.trap = fault.trap;
    std::memcpy(&pending.context, &context, sizeof(CONTEXT));
    pending.context.ContextFlags = CONTEXT_FULL | CONTEXT_SEGMENTS;
    context.Rsp = (context.Rsp - RedZone) & ~static_cast<DWORD64>(15);
    context.Rip = reinterpret_cast<DWORD64>(&Aps5FaultEntryStub);
    return true;
}

[[noreturn]] void FaultEntry(const void* vectors) {
    const PendingFault pending = pendingFault;
    pendingFault.handler = nullptr;
    CONTEXT context = pending.context;
    Deliver(pending.handler, pending.signum, context, pending.trap);
    Aps5RedirectedExit(&context, SaveVectors ? vectors : nullptr, RtlRestoreContext);
}

void InstallFaultDelivery() {
    static const bool installed = [] {
        LibcSetHostFaultHandler_nid_no_patch(DeliverHostFault);
        return true;
    }();
    (void)installed;
}
#else
constexpr int HostSignal = SIGUSR1;
constexpr std::size_t FxsaveRegisterBytes = 416;

void FromHost(const mcontext_t& host, GuestMcontext& m) {
    const auto* g = host.gregs;
    m.rdi = g[REG_RDI];
    m.rsi = g[REG_RSI];
    m.rdx = g[REG_RDX];
    m.rcx = g[REG_RCX];
    m.r8 = g[REG_R8];
    m.r9 = g[REG_R9];
    m.rax = g[REG_RAX];
    m.rbx = g[REG_RBX];
    m.rbp = g[REG_RBP];
    m.r10 = g[REG_R10];
    m.r11 = g[REG_R11];
    m.r12 = g[REG_R12];
    m.r13 = g[REG_R13];
    m.r14 = g[REG_R14];
    m.r15 = g[REG_R15];
    m.rip = g[REG_RIP];
    m.rsp = g[REG_RSP];
    m.rflags = g[REG_EFL];
    m.cs = static_cast<std::uint64_t>(g[REG_CSGSFS]) & 0xffffu;
    m.ss = static_cast<std::uint64_t>(g[REG_CSGSFS]) >> 48;
    m.len = sizeof(GuestMcontext);
    static_assert(sizeof(*host.fpregs) <= sizeof(m.fpstate) && FxsaveRegisterBytes <= sizeof(*host.fpregs));
    if (host.fpregs != nullptr) std::memcpy(m.fpstate, host.fpregs, sizeof(*host.fpregs));
}

void ToHost(const GuestMcontext& m, mcontext_t& host) {
    auto* g = host.gregs;
    g[REG_RDI] = static_cast<greg_t>(m.rdi);
    g[REG_RSI] = static_cast<greg_t>(m.rsi);
    g[REG_RDX] = static_cast<greg_t>(m.rdx);
    g[REG_RCX] = static_cast<greg_t>(m.rcx);
    g[REG_R8] = static_cast<greg_t>(m.r8);
    g[REG_R9] = static_cast<greg_t>(m.r9);
    g[REG_RAX] = static_cast<greg_t>(m.rax);
    g[REG_RBX] = static_cast<greg_t>(m.rbx);
    g[REG_RBP] = static_cast<greg_t>(m.rbp);
    g[REG_R10] = static_cast<greg_t>(m.r10);
    g[REG_R11] = static_cast<greg_t>(m.r11);
    g[REG_R12] = static_cast<greg_t>(m.r12);
    g[REG_R13] = static_cast<greg_t>(m.r13);
    g[REG_R14] = static_cast<greg_t>(m.r14);
    g[REG_R15] = static_cast<greg_t>(m.r15);
    g[REG_RIP] = static_cast<greg_t>(m.rip);
    g[REG_RSP] = static_cast<greg_t>(m.rsp);
    g[REG_EFL] = static_cast<greg_t>(m.rflags);
    if (host.fpregs != nullptr) std::memcpy(host.fpregs, m.fpstate, FxsaveRegisterBytes);
}

void Deliver(int, siginfo_t*, void* context) {
    const auto handler = Handler(RaisedSignal);
    if (handler == nullptr) {
        static constexpr char message[] = "sceKernelRaiseException: the handler was removed before the signal was delivered\n";
        const auto written = write(STDERR_FILENO, message, sizeof(message) - 1);
        (void)written;
        std::abort();
    }
    auto& host = static_cast<ucontext_t*>(context)->uc_mcontext;
    GuestUcontext ucontext{};
    FromHost(host, ucontext.mcontext);
    CallHandler(handler, RaisedSignal, ucontext);
    ToHost(ucontext.mcontext, host);
}

#ifdef __linux__
constexpr std::array<int, 4> FaultSignals{SIGSEGV, SIGBUS, SIGILL, SIGFPE};
constexpr greg_t VectorDivide = 0;
constexpr greg_t VectorInvalidOpcode = 6;
constexpr greg_t VectorSegmentNotPresent = 11;
constexpr greg_t VectorStack = 12;
constexpr greg_t VectorProtection = 13;
constexpr greg_t VectorPage = 14;
constexpr greg_t VectorFloatingPoint = 16;
constexpr greg_t VectorAlignment = 17;
constexpr greg_t VectorSimd = 19;
constexpr std::string_view GuestModuleSuffix = ".guest.prx";

std::array<struct sigaction, FaultSignals.size()> previousFaultActions{};

bool Classify(int hostSignal, const mcontext_t& host, Fault& fault) {
    const auto* g = host.gregs;
    const auto error = static_cast<std::uint64_t>(g[REG_ERR]);
    switch (g[REG_TRAPNO]) {
    case VectorDivide:
        fault = {SignalFloatingPoint, {TrapDivide, 0, 0}};
        return hostSignal == SIGFPE;
    case VectorInvalidOpcode:
        fault = {SignalIllegal, {TrapPrivilegedInstruction, 0, 0}};
        return hostSignal == SIGILL;
    case VectorSegmentNotPresent:
        fault = {SignalBus, {TrapSegmentNotPresent, 0, error}};
        return true;
    case VectorStack:
        fault = {SignalBus, {TrapStack, 0, error}};
        return true;
    case VectorProtection:
        fault = {SignalBus, {TrapProtection, 0, error}};
        return true;
    case VectorPage:
        fault = {hostSignal == SIGBUS ? SignalBus : SignalSegmentation, {TrapPage, static_cast<std::uint64_t>(g[REG_CR2]), error}};
        return hostSignal == SIGSEGV || hostSignal == SIGBUS;
    case VectorFloatingPoint:
        fault = {SignalFloatingPoint, {TrapArithmetic, 0, 0}};
        return hostSignal == SIGFPE;
    case VectorAlignment:
        fault = {SignalBus, {TrapAlignment, 0, error}};
        return true;
    case VectorSimd:
        fault = {SignalFloatingPoint, {TrapSimd, 0, 0}};
        return hostSignal == SIGFPE;
    default:
        return false;
    }
}

bool GuestCode(std::uintptr_t rip) {
    if (GuestArena::GuestArenaContains_nid_postfix(reinterpret_cast<const void*>(rip), 1)) return true;
#ifdef LIBKERNEL_EXCEPTION_FIND_OBJECT
    dl_find_object object{};
    if (_dl_find_object(reinterpret_cast<void*>(rip), &object) != 0) return false;
    const link_map* native = object.dlfo_link_map;
#else
    Dl_info symbol{};
    link_map* native = nullptr;
    if (!dladdr1(reinterpret_cast<const void*>(rip), &symbol, reinterpret_cast<void**>(&native), RTLD_DL_LINKMAP)) return false;
#endif
    if (!native) return false;
    const std::string_view name = native->l_name ? native->l_name : "";
    return name.empty() || name.ends_with(GuestModuleSuffix);
}

void ChainFault(int hostSignal, siginfo_t* info, void* context) {
    for (std::size_t i = 0; i < FaultSignals.size(); ++i) {
        if (FaultSignals[i] != hostSignal) continue;
        const auto& previous = previousFaultActions[i];
        if ((previous.sa_flags & SA_SIGINFO) != 0 && previous.sa_sigaction != nullptr) {
            previous.sa_sigaction(hostSignal, info, context);
            return;
        }
        if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN && previous.sa_handler != nullptr) {
            previous.sa_handler(hostSignal);
            return;
        }
        if (info->si_code <= 0 && previous.sa_handler == SIG_IGN) return;
    }
    ::signal(hostSignal, SIG_DFL);
    if (info->si_code <= 0) ::raise(hostSignal);
}

void DeliverFault(int hostSignal, siginfo_t* info, void* context) {
    auto& host = static_cast<ucontext_t*>(context)->uc_mcontext;
    Fault fault{};
    const bool deliverable = info->si_code > 0 && Classify(hostSignal, host, fault);
    if (deliverable && fault.trap.number == TrapPage && GuestWriteWatch::GuestWriteWatchHandleFault_nid_postfix(fault.trap.address)) return;
    const auto handler = deliverable ? Handler(fault.signum) : nullptr;
    if (handler == nullptr || !GuestCode(static_cast<std::uintptr_t>(host.gregs[REG_RIP]))) {
        ChainFault(hostSignal, info, context);
        return;
    }
    GuestUcontext ucontext{};
    FromHost(host, ucontext.mcontext);
    CallHandler(handler, fault.signum, ucontext, fault.trap);
    ToHost(ucontext.mcontext, host);
}

void InstallFaultDelivery() {
    static const bool installed = [] {
        struct sigaction action{};
        action.sa_sigaction = DeliverFault;
        action.sa_flags = SA_SIGINFO | SA_NODEFER;
        sigemptyset(&action.sa_mask);
        for (std::size_t i = 0; i < FaultSignals.size(); ++i)
            if (sigaction(FaultSignals[i], &action, &previousFaultActions[i]) != 0)
                throw std::system_error(errno, std::generic_category(), "sceKernelInstallExceptionHandler: cannot install the host fault handler");
        return true;
    }();
    (void)installed;
}
#endif

void InstallDelivery() {
    static const bool installed = [] {
        struct sigaction action{};
        action.sa_sigaction = Deliver;
        action.sa_flags = SA_SIGINFO | SA_RESTART;
        sigemptyset(&action.sa_mask);
        if (sigaction(HostSignal, &action, nullptr) != 0)
            throw std::system_error(errno, std::generic_category(), "sceKernelRaiseException: cannot install the host signal handler");
        return true;
    }();
    (void)installed;
}

bool Send(Pthread thread) {
    const int result = pthread_kill(thread->hostThread, HostSignal);
    if (result == ESRCH) return false;
    if (result != 0) throw std::system_error(result, std::generic_category(), "sceKernelRaiseException: cannot signal the target thread");
    return true;
}

class HostSignalBlock {
public:
    HostSignalBlock() {
        sigset_t blocked;
        sigemptyset(&blocked);
        sigaddset(&blocked, HostSignal);
        pthread_sigmask(SIG_BLOCK, &blocked, &previous);
    }
    ~HostSignalBlock() { pthread_sigmask(SIG_SETMASK, &previous, nullptr); }
    HostSignalBlock(const HostSignalBlock&) = delete;
    HostSignalBlock& operator=(const HostSignalBlock&) = delete;

private:
    sigset_t previous;
};

bool RaiseOn(Pthread thread) {
    InstallDelivery();
    if (thread == scePthreadSelf()) return Send(thread);
    HostSignalBlock block;
    std::lock_guard lock(thread->_join_mtx);
    if (thread->_finished.load(std::memory_order_acquire)) return false;
    return Send(thread);
}
#endif

}

#ifdef _WIN32
extern "C" [[noreturn]] void Aps5RedirectedEntry(void* delivery) {
    RedirectedEntry(static_cast<Delivery*>(delivery));
}
extern "C" [[noreturn]] void Aps5FaultEntry(const void* vectors) {
    FaultEntry(vectors);
}
extern "C" const bool Aps5FaultSavesVectors = SaveVectors;
static_assert(VectorBytes == 512, "Aps5FaultEntryStub stores ymm0-ymm15 in 512 bytes below the redirected stack pointer");
asm(".text\n"
    ".globl Aps5FaultEntryStub\n"
    "Aps5FaultEntryStub:\n"
    "    subq $512, %rsp\n"
    "    cmpb $0, Aps5FaultSavesVectors(%rip)\n"
    "    je 1f\n"
    "    vmovdqu %ymm0, 0(%rsp)\n"
    "    vmovdqu %ymm1, 32(%rsp)\n"
    "    vmovdqu %ymm2, 64(%rsp)\n"
    "    vmovdqu %ymm3, 96(%rsp)\n"
    "    vmovdqu %ymm4, 128(%rsp)\n"
    "    vmovdqu %ymm5, 160(%rsp)\n"
    "    vmovdqu %ymm6, 192(%rsp)\n"
    "    vmovdqu %ymm7, 224(%rsp)\n"
    "    vmovdqu %ymm8, 256(%rsp)\n"
    "    vmovdqu %ymm9, 288(%rsp)\n"
    "    vmovdqu %ymm10, 320(%rsp)\n"
    "    vmovdqu %ymm11, 352(%rsp)\n"
    "    vmovdqu %ymm12, 384(%rsp)\n"
    "    vmovdqu %ymm13, 416(%rsp)\n"
    "    vmovdqu %ymm14, 448(%rsp)\n"
    "    vmovdqu %ymm15, 480(%rsp)\n"
    "1:\n"
    "    movq %rsp, %rcx\n"
    "    subq $40, %rsp\n"
    "    movq $0, (%rsp)\n"
    "    jmp Aps5FaultEntry\n");
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
 void* expected = nullptr;
 if (!handlers[signum].compare_exchange_strong(expected, handler, std::memory_order_acq_rel)) return SCE_KERNEL_ERROR_EAGAIN;
#if defined(_WIN32) || defined(__linux__)
 if (FaultSignal(signum)) InstallFaultDelivery();
#endif
 return 0;
}

int APS5_VABI sceKernelRemoveExceptionHandler(int signum) {
 if (!Allowed(signum)) return SCE_KERNEL_ERROR_EINVAL;
 handlers[signum].store(nullptr, std::memory_order_release);
 return 0;
}

int APS5_VABI sceKernelRaiseException(Pthread thread, int signum) {
 if (signum != RaisedSignal) return SCE_KERNEL_ERROR_EINVAL;
 if (thread == nullptr || thread->_finished.load(std::memory_order_acquire)) return SCE_KERNEL_ERROR_ESRCH;
 const auto handler = Handler(signum);
 if (handler == nullptr) throw std::runtime_error("sceKernelRaiseException: no handler installed for the signal");
#ifdef _WIN32
 return RaiseOn(thread, handler, signum) ? 0 : SCE_KERNEL_ERROR_ESRCH;
#else
 return RaiseOn(thread) ? 0 : SCE_KERNEL_ERROR_ESRCH;
#endif
}

void APS5_VABI sceKernelDebugRaiseException(int c1, int c2) {
    APS5_LOG_OUT("sceKernelDebugRaiseException c1=0x%08x c2=0x%08x caller=%p", static_cast<unsigned>(c1), static_cast<unsigned>(c2), __builtin_return_address(0));
}

void APS5_VABI sceKernelDebugRaiseExceptionOnReleaseMode(int c1, int c2) {
    APS5_LOG_OUT("sceKernelDebugRaiseExceptionOnReleaseMode c1=0x%08x c2=0x%08x caller=%p", static_cast<unsigned>(c1), static_cast<unsigned>(c2), __builtin_return_address(0));
}

}
