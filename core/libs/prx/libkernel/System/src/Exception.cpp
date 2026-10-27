#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <vector>
#include <algorithm>
#include <array>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <cstdio>
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

void CallHandler(GuestExceptionHandler handler, int signum, GuestUcontext& ucontext) {
    auto& m = ucontext.mcontext;
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
    CallHandler(handler, signum, ucontext);
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
    Deliver(delivery->handler, delivery->signum, context);
    Aps5RedirectedExit(&context, delivery->saveVectors != 0 ? delivery->vectors : nullptr, RtlRestoreContext);
}

bool CodeReadable(DWORD64 low, DWORD64 high) {
    constexpr DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    for (DWORD64 address = low; address < high;) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) != sizeof(info)) return false;
        if (info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 || (info.Protect & readable) == 0) return false;
        address = reinterpret_cast<DWORD64>(info.BaseAddress) + info.RegionSize;
    }
    return true;
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

struct QueuedDelivery {
    GuestExceptionHandler handler;
    int signum;
    std::atomic<bool> taken{false};
    std::atomic<bool> waiting{false};
    std::atomic<int> references{1};
};

void ReleaseQueued(QueuedDelivery* queued) {
    if (queued != nullptr && queued->references.fetch_sub(1, std::memory_order_acq_rel) == 1) delete queued;
}

void CALLBACK WaitingEntry(ULONG_PTR parameter) {
    auto* queued = reinterpret_cast<QueuedDelivery*>(parameter);
    const auto handler = queued->handler;
    const int signum = queued->signum;
    const bool claimed = !queued->taken.exchange(true, std::memory_order_acq_rel);
    ReleaseQueued(queued);
    if (!claimed) return;
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

CodeKind RunningCode(DWORD64 rip, DWORD64& base) {
    base = ImageBase(rip);
    auto kind = CachedKind(base);
    if (kind == CodeKind::Unknown && Images().count.load(std::memory_order_acquire) >= ImageKinds::Capacity) kind = CodeKind::Host;
    return kind;
}

enum class Attempt : std::uint8_t { Delivered, Exited, Host, Restoring };

bool QueueWaitingDelivery(HANDLE native, QueuedDelivery* queued) {
    if (queued->waiting.exchange(true, std::memory_order_acq_rel)) return true;
    queued->references.fetch_add(1, std::memory_order_relaxed);
    if (QueueUserAPC(WaitingEntry, native, reinterpret_cast<ULONG_PTR>(queued)) != 0) return true;
    queued->references.fetch_sub(1, std::memory_order_relaxed);
    queued->waiting.store(false, std::memory_order_release);
    return false;
}

void Redirect(HANDLE native, Delivery& delivery) {
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
}

const RUNTIME_FUNCTION* ImageFunction(DWORD64 image, DWORD64 rip) {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto* headers = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + static_cast<DWORD64>(dos->e_lfanew));
    const auto& directory = headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (directory.VirtualAddress == 0 || directory.Size < sizeof(RUNTIME_FUNCTION)) return nullptr;
    const auto* functions = reinterpret_cast<const RUNTIME_FUNCTION*>(image + directory.VirtualAddress);
    const auto count = directory.Size / sizeof(RUNTIME_FUNCTION);
    const auto offset = static_cast<DWORD>(rip - image);
    std::size_t low = 0;
    std::size_t high = count;
    while (low < high) {
        const auto middle = low + (high - low) / 2;
        if (offset < functions[middle].BeginAddress) high = middle;
        else if (offset >= functions[middle].EndAddress) low = middle + 1;
        else return &functions[middle];
    }
    return nullptr;
}

bool PushesMachineFrame(DWORD64 image, const RUNTIME_FUNCTION* function) {
    constexpr std::uint8_t ChainInfo = 4;
    constexpr std::uint8_t PushMachineFrame = 10;
    for (int links = 0; links < 32; ++links) {
        const auto* info = reinterpret_cast<const std::uint8_t*>(image + function->UnwindData);
        const std::uint8_t flags = info[0] >> 3;
        const std::uint8_t codes = info[2];
        const auto* code = info + 4;
        for (std::uint8_t index = 0; index < codes; ++index) {
            if ((code[index * 2 + 1] & 0x0f) == PushMachineFrame) return true;
        }
        if ((flags & ChainInfo) == 0) return false;
        function = reinterpret_cast<const RUNTIME_FUNCTION*>(code + ((codes + 1u) & ~1u) * 2);
    }
    return true;
}

bool ReturnSite(DWORD64 at) {
    if (at < 7 || !CodeReadable(at - 7, at)) return false;
    const auto* code = reinterpret_cast<const std::uint8_t*>(at);
    const auto callModrm = [](std::uint8_t modrm) { return (modrm & 0x38u) == 0x10u; };
    if (code[-5] == 0xe8u) return true;
    if (code[-2] == 0xffu && callModrm(code[-1]) && (code[-1] & 0xc0u) == 0xc0u) return true;
    if (code[-3] == 0xffu && callModrm(code[-2])) return true;
    if (code[-6] == 0xffu && callModrm(code[-5])) return true;
    return code[-7] == 0xffu && callModrm(code[-6]);
}

void*** UnwindGuard() {
    static thread_local void** guard = nullptr;
    return &guard;
}

[[noreturn]] void UnwindRecovery() {
    __builtin_longjmp(*UnwindGuard(), 1);
}

bool UnwindToGuest(CONTEXT& context, DWORD64& at, DWORD64& unknown) {
    for (int depth = 0; depth < 64; ++depth) {
        const auto image = ImageBase(context.Rip);
        const auto kind = CachedKind(image);
        if (kind == CodeKind::Guest) {
            at = context.Rip;
            return depth != 0 && ReturnSite(at);
        }
        if (kind == CodeKind::Unknown && image != 0) unknown = image;
        if (image == 0 || kind != CodeKind::Host) return false;
        const auto rsp = context.Rsp;
        if (!StackWritable(rsp, rsp + sizeof(DWORD64))) return false;
        if (const auto* function = ImageFunction(image, context.Rip)) {
            if (PushesMachineFrame(image, function)) return false;
            void* handlerData = nullptr;
            DWORD64 establisher = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, context.Rip, const_cast<RUNTIME_FUNCTION*>(function), &context, &handlerData, &establisher, nullptr);
            if (context.Rsp <= rsp) return false;
        } else {
            if (!StackWritable(context.Rsp, context.Rsp + sizeof(DWORD64))) return false;
            context.Rip = *reinterpret_cast<const DWORD64*>(context.Rsp);
            context.Rsp += sizeof(DWORD64);
        }
    }
    return false;
}

bool GuestReturn(CONTEXT context, DWORD64& at, DWORD64& unknown) {
    void* buffer[5]{};
    if (__builtin_setjmp(buffer) != 0) {
        *UnwindGuard() = nullptr;
        return false;
    }
    *UnwindGuard() = buffer;
    const bool found = UnwindToGuest(context, at, unknown);
    *UnwindGuard() = nullptr;
    return found;
}

constexpr DWORD ReservedThread = 0xffffffffu;
constexpr DWORD64 ReturnBreakBits = 0xf0003ull;

struct ReturnBreak {
    std::atomic<DWORD> thread{0};
    DWORD64 at = 0;
    QueuedDelivery* queued = nullptr;
};

std::atomic<bool>& ReturnBreaksInstalled() {
    static std::atomic<bool> installed{false};
    return installed;
}

std::array<ReturnBreak, 256>& ReturnBreaks() {
    static std::array<ReturnBreak, 256> breaks;
    return breaks;
}

struct ArmedBreak {
    std::atomic<DWORD64> at{0};
    std::atomic<DWORD> thread{0};
};

std::array<ArmedBreak, 256>& ArmedBreaks() {
    static std::array<ArmedBreak, 256> armed;
    return armed;
}

std::atomic<std::size_t>& ArmedCount() {
    static std::atomic<std::size_t> count{0};
    return count;
}

void NoteArmedBreak(DWORD thread, DWORD64 at) {
    auto& slot = ArmedBreaks()[ArmedCount().fetch_add(1, std::memory_order_relaxed) % ArmedBreaks().size()];
    slot.at.store(0, std::memory_order_release);
    slot.thread.store(thread, std::memory_order_release);
    slot.at.store(at, std::memory_order_release);
}

bool ArmedOnThread(DWORD thread, DWORD64 at) {
    if (at == 0) return false;
    for (const auto& armed : ArmedBreaks()) {
        if (armed.at.load(std::memory_order_acquire) != at) continue;
        const DWORD owner = armed.thread.load(std::memory_order_acquire);
        if (owner == thread && armed.at.load(std::memory_order_acquire) == at) return true;
    }
    return false;
}

bool DisarmedReturnBreak(const EXCEPTION_POINTERS& info) {
    const auto& context = *info.ContextRecord;
    constexpr DWORD TrapFlag = 0x100u;
    constexpr DWORD64 BreakpointHits = 0xfu;
    constexpr DWORD64 EnabledBreakpoints = 0xffu;
    if ((context.EFlags & TrapFlag) != 0) return false;
    if ((context.ContextFlags & CONTEXT_DEBUG_REGISTERS) == CONTEXT_DEBUG_REGISTERS && ((context.Dr6 & BreakpointHits) != 0 || (context.Dr7 & EnabledBreakpoints) != 0)) return false;
    const auto at = reinterpret_cast<DWORD64>(info.ExceptionRecord->ExceptionAddress);
    return at == context.Rip && ArmedOnThread(GetCurrentThreadId(), at);
}

QueuedDelivery* ClaimReturnBreak(ReturnBreak& entry, DWORD thread) {
    DWORD expected = thread;
    if (!entry.thread.compare_exchange_strong(expected, ReservedThread, std::memory_order_acq_rel)) return nullptr;
    auto* queued = entry.queued;
    entry.thread.store(0, std::memory_order_release);
    return queued;
}

[[noreturn]] void ReturnBreakEntry(Delivery* stored) {
    CONTEXT context = stored->context;
    const auto handler = stored->handler;
    const int signum = stored->signum;
    delete stored;
    Deliver(handler, signum, context);
    Aps5RedirectedExit(&context, nullptr, RtlRestoreContext);
}

LONG CALLBACK ReturnBreakHandler(EXCEPTION_POINTERS* info) {
    auto* context = info->ContextRecord;
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && *UnwindGuard() != nullptr) {
        context->Rsp = (context->Rsp & ~static_cast<DWORD64>(15)) - 8;
        context->Rip = reinterpret_cast<DWORD64>(&UnwindRecovery);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    if ((context->Dr6 & 1u) == 0) return DisarmedReturnBreak(*info) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
    const DWORD64 at = context->Dr0;
    const DWORD self = GetCurrentThreadId();
    QueuedDelivery* queued = nullptr;
    for (auto& entry : ReturnBreaks()) {
        if (entry.thread.load(std::memory_order_acquire) != self || entry.at != at) continue;
        queued = ClaimReturnBreak(entry, self);
        if (queued != nullptr) break;
    }
    if (queued == nullptr && !ArmedOnThread(self, at)) return EXCEPTION_CONTINUE_SEARCH;
    context->Dr0 = 0;
    context->Dr6 = 0;
    context->Dr7 &= ~ReturnBreakBits;
    context->ContextFlags |= CONTEXT_DEBUG_REGISTERS;
    if (queued == nullptr) return EXCEPTION_CONTINUE_EXECUTION;
    if (context->Rip == at && !queued->taken.exchange(true, std::memory_order_acq_rel)) {
        auto* stored = new Delivery{queued->handler, queued->signum, *context, {}, 0};
        stored->context.ContextFlags = CONTEXT_FULL | CONTEXT_SEGMENTS;
        stored->context.Dr0 = stored->context.Dr1 = stored->context.Dr2 = stored->context.Dr3 = stored->context.Dr6 = stored->context.Dr7 = 0;
        context->Rsp = ((context->Rsp - RedZone - 64) & ~static_cast<DWORD64>(15)) - 8;
        context->Rip = reinterpret_cast<DWORD64>(&ReturnBreakEntry);
        context->Rcx = reinterpret_cast<DWORD64>(stored);
    }
    ReleaseQueued(queued);
    return EXCEPTION_CONTINUE_EXECUTION;
}

bool InstallReturnBreaks() {
    static const bool installed = [] {
        const bool added = AddVectoredExceptionHandler(1, ReturnBreakHandler) != nullptr;
        ReturnBreaksInstalled().store(added, std::memory_order_release);
        return added;
    }();
    return installed;
}

bool SetReturnBreak(HANDLE native, DWORD64 at, bool armed) {
    CONTEXT debug{};
    debug.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(native, &debug)) return false;
    if (!armed && debug.Dr0 != at) return true;
    debug.Dr0 = armed ? at : 0;
    debug.Dr7 = (debug.Dr7 & ~ReturnBreakBits) | (armed ? 1ull : 0ull);
    debug.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    return SetThreadContext(native, &debug) != 0;
}

struct ReturnArm {
    ReturnBreak* entry = nullptr;
    DWORD thread = 0;
    DWORD64 at = 0;
};

void ArmReturnBreak(HANDLE native, const CONTEXT& context, QueuedDelivery* queued, ReturnArm& arm, DWORD64& unknown) {
    if (!ReturnBreaksInstalled().load(std::memory_order_acquire) || queued == nullptr) return;
    DWORD64 at = 0;
    if (!GuestReturn(context, at, unknown)) return;
    if (arm.entry != nullptr && arm.at == at) return;
    if (arm.entry != nullptr) {
        static_cast<void>(SetReturnBreak(native, arm.at, false));
        ReleaseQueued(ClaimReturnBreak(*arm.entry, arm.thread));
        arm = {};
    }
    const DWORD thread = GetThreadId(native);
    for (auto& entry : ReturnBreaks()) {
        DWORD expected = 0;
        if (!entry.thread.compare_exchange_strong(expected, ReservedThread, std::memory_order_acq_rel)) continue;
        entry.at = at;
        entry.queued = queued;
        queued->references.fetch_add(1, std::memory_order_relaxed);
        entry.thread.store(thread, std::memory_order_release);
        NoteArmedBreak(thread, at);
        if (!SetReturnBreak(native, at, true)) {
            ReleaseQueued(ClaimReturnBreak(entry, thread));
            return;
        }
        arm = {&entry, thread, at};
        return;
    }
}

void DisarmReturnBreak(HANDLE native, ReturnArm& arm) {
    if (arm.entry == nullptr) return;
    const bool suspended = SuspendThread(native) != static_cast<DWORD>(-1);
    if (suspended && !Exited(native)) static_cast<void>(SetReturnBreak(native, arm.at, false));
    ReleaseQueued(ClaimReturnBreak(*arm.entry, arm.thread));
    if (suspended) ResumeThread(native);
    arm = {};
}

Attempt TryDeliver(Pthread thread, HANDLE native, GuestExceptionHandler handler, int signum, QueuedDelivery*& queued, ReturnArm* arm = nullptr) {
    alignas(16) Delivery delivery{handler, signum, {}, {}, SaveVectors ? 1u : 0u};
    const auto contextDeadline = std::chrono::steady_clock::now() + RetryLimit;
    for (;;) {
        if (SuspendThread(native) == static_cast<DWORD>(-1)) {
            if (Exited(native)) return Attempt::Exited;
            throw std::runtime_error("sceKernelRaiseException: cannot suspend the target thread");
        }
        if (Exited(native)) {
            ResumeThread(native);
            return Attempt::Exited;
        }
        if (queued != nullptr && queued->taken.load(std::memory_order_acquire)) {
            ResumeThread(native);
            return Attempt::Delivered;
        }
        delivery.context.ContextFlags = CONTEXT_FULL | CONTEXT_SEGMENTS;
        if (!GetThreadContext(native, &delivery.context)) {
            ResumeThread(native);
            if (std::chrono::steady_clock::now() >= contextDeadline) throw std::runtime_error("sceKernelRaiseException: cannot read the target thread context");
            SwitchToThread();
            continue;
        }
        if (thread->waitCount.load(std::memory_order_seq_cst) > 0) {
            if (queued == nullptr) queued = new QueuedDelivery{handler, signum};
            const bool accepted = QueueWaitingDelivery(native, queued);
            ResumeThread(native);
            if (!accepted) throw std::runtime_error("sceKernelRaiseException: cannot queue delivery to the waiting thread");
            return Attempt::Delivered;
        }
        const bool restoring = RestoringContext(delivery.context.Rip);
        DWORD64 image = 0;
        const auto running = restoring ? CodeKind::Host : RunningCode(delivery.context.Rip, image);
        if (running == CodeKind::Guest && arm != nullptr && arm->entry != nullptr && delivery.context.Rip == arm->at) {
            ResumeThread(native);
            return Attempt::Host;
        }
        if (running == CodeKind::Guest) {
            if (queued != nullptr && queued->taken.exchange(true, std::memory_order_acq_rel)) {
                ResumeThread(native);
                return Attempt::Delivered;
            }
            Redirect(native, delivery);
            return Attempt::Delivered;
        }
        if (running == CodeKind::Host && queued == nullptr) queued = new QueuedDelivery{handler, signum};
        DWORD64 unknown = 0;
        if (arm != nullptr && running == CodeKind::Host && !restoring) ArmReturnBreak(native, delivery.context, queued, *arm, unknown);
        ResumeThread(native);
        if (running == CodeKind::Unknown) {
            static_cast<void>(ClassifyImage(image));
            continue;
        }
        if (unknown != 0) {
            static_cast<void>(ClassifyImage(unknown));
            continue;
        }
        return restoring ? Attempt::Restoring : Attempt::Host;
    }
}

struct PendingRaise {
    Pthread thread;
    HANDLE native;
    GuestExceptionHandler handler;
    int signum;
    QueuedDelivery* queued;
    ReturnArm arm{};
};

class RaiseDeliverer {
public:
    void Add(PendingRaise raise) {
        {
            std::lock_guard lock(mutex);
            pending.push_back(raise);
            if (!started) {
                started = true;
                std::thread([this] { run(); }).detach();
            }
        }
        wake.notify_one();
    }

private:
    void run() {
        std::vector<PendingRaise> sweep;
        unsigned idleSweeps = 0;
        for (;;) {
            {
                std::unique_lock lock(mutex);
                if (sweep.empty()) {
                    wake.wait(lock, [this] { return !pending.empty(); });
                    idleSweeps = 0;
                }
                sweep.insert(sweep.end(), pending.begin(), pending.end());
                pending.clear();
            }
            std::vector<PendingRaise> kept;
            for (auto& raise : sweep) {
                Attempt attempt;
                try {
                    attempt = TryDeliver(raise.thread, raise.native, raise.handler, raise.signum, raise.queued, &raise.arm);
                } catch (const std::exception& error) {
                    std::fprintf(stderr, "[exception] signal 0x%x: %s\n", raise.signum, error.what());
                    std::abort();
                }
                if (attempt == Attempt::Host || attempt == Attempt::Restoring) {
                    kept.push_back(raise);
                    continue;
                }
                DisarmReturnBreak(raise.native, raise.arm);
                ReleaseQueued(raise.queued);
                CloseHandle(raise.native);
            }
            sweep = std::move(kept);
            if (sweep.empty()) continue;
            const bool armed = std::all_of(sweep.begin(), sweep.end(), [](const PendingRaise& raise) { return raise.arm.entry != nullptr; });
            if (!armed && ++idleSweeps < 64) SwitchToThread();
            else Sleep(1);
        }
    }

    std::mutex mutex;
    std::condition_variable wake;
    std::vector<PendingRaise> pending;
    bool started = false;
};

RaiseDeliverer& Deliverer() {
    static auto& deliverer = *new RaiseDeliverer();
    return deliverer;
}

bool RaiseOn(Pthread thread, GuestExceptionHandler handler, int signum) {
    if (thread == scePthreadSelf()) {
        CONTEXT context{};
        RtlCaptureContext(&context);
        Deliver(handler, signum, context);
        return true;
    }
    const auto native = static_cast<HANDLE>(thread->nativeHandle);
    const auto retryDeadline = std::chrono::steady_clock::now() + RetryLimit;
    static_cast<void>(InstallReturnBreaks());
    static_cast<void>(RestoringContext(0));
    static_cast<void>(ExecutableBase());
    static_cast<void>(Images());
    QueuedDelivery* queued = nullptr;
    ReturnArm arm;
    for (;;) {
        const auto attempt = TryDeliver(thread, native, handler, signum, queued, &arm);
        if (attempt == Attempt::Delivered || attempt == Attempt::Exited) {
            DisarmReturnBreak(native, arm);
            ReleaseQueued(queued);
            return attempt == Attempt::Delivered;
        }
        const auto now = std::chrono::steady_clock::now();
        if (attempt == Attempt::Restoring && now >= retryDeadline) {
            const bool taken = queued != nullptr && queued->taken.exchange(true, std::memory_order_acq_rel);
            DisarmReturnBreak(native, arm);
            ReleaseQueued(queued);
            if (taken) return true;
            throw std::runtime_error("sceKernelRaiseException: the target thread stayed inside NtContinue for 1 s");
        }
        if (attempt == Attempt::Host) {
            HANDLE owned = nullptr;
            if (!DuplicateHandle(GetCurrentProcess(), native, GetCurrentProcess(), &owned, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
                DisarmReturnBreak(native, arm);
                ReleaseQueued(queued);
                throw std::runtime_error("sceKernelRaiseException: cannot keep the target thread for a later delivery");
            }
            Deliverer().Add({thread, owned, handler, signum, queued, arm});
            return true;
        }
        SwitchToThread();
    }
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

void Deliver(int signum, siginfo_t*, void* context) {
    if (TimedWait::DeferDelivery(signum)) return;
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
 if (thread->suspended.load(std::memory_order_acquire)) throw std::runtime_error("sceKernelRaiseException: the target thread is still suspended at creation");
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
