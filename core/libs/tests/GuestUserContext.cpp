#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/System/include/UserContext.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <xmmintrin.h>
#ifdef _WIN32
#include <windows.h>
#endif

using ContextEntry = void (APS5_VABI*)();

extern "C" {

int APS5_VABI CaptureKnownRegisters(UserUcontext* context);
void CaptureReturnAddress();
void RecordRegisters();
void APS5_VABI EntryThunk();

std::uint64_t CapturedStackPointer = 0;
std::uint64_t EntryStackPointer = 0;
std::uint64_t RecordedRegisters[18] = {};
std::uint32_t RecordedMxcsr = 0;
std::uint16_t RecordedControlWord = 0;
alignas(16) std::uint8_t RecordedXmm[16 * 16] = {};
UserUcontext ReturnContext = {};
}

asm(".text\n"
    APS5_ASM_FUNCTION("CaptureKnownRegisters")
    "    push %rbx\n"
    "    push %rbp\n"
    "    push %r12\n"
    "    push %r13\n"
    "    push %r14\n"
    "    push %r15\n"
    "    sub $8, %rsp\n"
    "    movabsq $0x1111111111111111, %rbx\n"
    "    movabsq $0x2222222222222222, %rbp\n"
    "    movabsq $0x3333333333333333, %r12\n"
    "    movabsq $0x4444444444444444, %r13\n"
    "    movabsq $0x5555555555555555, %r14\n"
    "    movabsq $0x6666666666666666, %r15\n"
    "    movabsq $0x7777777777777777, %rcx\n"
    "    movabsq $0x8888888888888888, %rsi\n"
    "    movabsq $0x9999999999999999, %r8\n"
    "    movabsq $0xaaaaaaaaaaaaaaaa, %r9\n"
    "    movabsq $0xbbbbbbbbbbbbbbbb, %r10\n"
    "    movabsq $0xcccccccccccccccc, %r11\n"
    "    movabsq $0xdddddddddddddddd, %rdx\n"
    "    movabsq $0xeeeeeeeeeeeeeeee, %rax\n"
    "    mov %rsp, " APS5_ASM_SYMBOL("CapturedStackPointer") "(%rip)\n"
    "    stc\n"
    "    call " APS5_ASM_SYMBOL("getcontext_nid_postfix") "\n"
    APS5_ASM_FUNCTION("CaptureReturnAddress")
    "    add $8, %rsp\n"
    "    pop %r15\n"
    "    pop %r14\n"
    "    pop %r13\n"
    "    pop %r12\n"
    "    pop %rbp\n"
    "    pop %rbx\n"
    "    ret\n"
    APS5_ASM_FUNCTION("RecordRegisters")
    "    pushfq\n"
    "    mov %rax, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x00(%rip)\n"
    "    pop %rax\n"
    "    mov %rax, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x88(%rip)\n"
    "    mov %rbx, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x08(%rip)\n"
    "    mov %rcx, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x10(%rip)\n"
    "    mov %rdx, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x18(%rip)\n"
    "    mov %rsi, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x20(%rip)\n"
    "    mov %rdi, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x28(%rip)\n"
    "    mov %rbp, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x30(%rip)\n"
    "    mov %rsp, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x38(%rip)\n"
    "    mov %r8, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x40(%rip)\n"
    "    mov %r9, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x48(%rip)\n"
    "    mov %r10, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x50(%rip)\n"
    "    mov %r11, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x58(%rip)\n"
    "    mov %r12, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x60(%rip)\n"
    "    mov %r13, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x68(%rip)\n"
    "    mov %r14, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x70(%rip)\n"
    "    mov %r15, " APS5_ASM_SYMBOL("RecordedRegisters") "+0x78(%rip)\n"
    "    stmxcsr " APS5_ASM_SYMBOL("RecordedMxcsr") "(%rip)\n"
    "    fnstcw " APS5_ASM_SYMBOL("RecordedControlWord") "(%rip)\n"
    "    movdqu %xmm0, " APS5_ASM_SYMBOL("RecordedXmm") "+0x00(%rip)\n"
    "    movdqu %xmm1, " APS5_ASM_SYMBOL("RecordedXmm") "+0x10(%rip)\n"
    "    movdqu %xmm2, " APS5_ASM_SYMBOL("RecordedXmm") "+0x20(%rip)\n"
    "    movdqu %xmm3, " APS5_ASM_SYMBOL("RecordedXmm") "+0x30(%rip)\n"
    "    movdqu %xmm4, " APS5_ASM_SYMBOL("RecordedXmm") "+0x40(%rip)\n"
    "    movdqu %xmm5, " APS5_ASM_SYMBOL("RecordedXmm") "+0x50(%rip)\n"
    "    movdqu %xmm6, " APS5_ASM_SYMBOL("RecordedXmm") "+0x60(%rip)\n"
    "    movdqu %xmm7, " APS5_ASM_SYMBOL("RecordedXmm") "+0x70(%rip)\n"
    "    movdqu %xmm8, " APS5_ASM_SYMBOL("RecordedXmm") "+0x80(%rip)\n"
    "    movdqu %xmm9, " APS5_ASM_SYMBOL("RecordedXmm") "+0x90(%rip)\n"
    "    movdqu %xmm10, " APS5_ASM_SYMBOL("RecordedXmm") "+0xa0(%rip)\n"
    "    movdqu %xmm11, " APS5_ASM_SYMBOL("RecordedXmm") "+0xb0(%rip)\n"
    "    movdqu %xmm12, " APS5_ASM_SYMBOL("RecordedXmm") "+0xc0(%rip)\n"
    "    movdqu %xmm13, " APS5_ASM_SYMBOL("RecordedXmm") "+0xd0(%rip)\n"
    "    movdqu %xmm14, " APS5_ASM_SYMBOL("RecordedXmm") "+0xe0(%rip)\n"
    "    movdqu %xmm15, " APS5_ASM_SYMBOL("RecordedXmm") "+0xf0(%rip)\n"
    "    cld\n"
    "    lea " APS5_ASM_SYMBOL("ReturnContext") "(%rip), %rdi\n"
    "    and $-16, %rsp\n"
    "    call " APS5_ASM_SYMBOL("setcontext_nid_postfix") "\n"
    "    ud2\n"
    APS5_ASM_FUNCTION("EntryThunk")
    "    mov %rsp, " APS5_ASM_SYMBOL("EntryStackPointer") "(%rip)\n"
    "    jmp " APS5_ASM_SYMBOL("ContextBody") "\n");

namespace {

constexpr int GuestEinval = 22;
constexpr int GuestSigBlock = 1;
constexpr int GuestSigSetmask = 3;
constexpr std::uint64_t UserChangeableFlags = 0x254dd5;
constexpr std::uint64_t InterruptFlag = 0x200;
constexpr std::size_t FxsaveMxcsr = 24;
constexpr std::size_t FxsaveXmm = 160;
constexpr std::size_t StackBytes = 256 * 1024;

struct alignas(16) Stack {
    unsigned char bytes[StackBytes];
};

struct Fibre {
    UserUcontext context;
    std::uint64_t jump[12];
    int started;
};

Stack g_restoreStack;
Stack g_contextStack;
Stack g_jobStack;
Stack g_exitStack;
std::uint64_t g_arguments[6] = {};
int g_entries = 0;
bool g_contextChainEnded = false;
Fibre g_dispatcher = {};
Fibre g_job = {};
int g_jobRuns = 0;
bool g_signalJumps = false;

void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "User context check failed: %s\n", what);
        std::abort();
    }
}

std::uint16_t ControlWord() {
    std::uint16_t control = 0;
    asm volatile("fnstcw %0" : "=m"(control));
    return control;
}

std::uint64_t Pattern(unsigned index) {
    return 0x0101010101010101ull * (index + 1) + 0x0f00000000000000ull;
}

void CheckStackBounds(const Stack* stack, const char* what) {
#ifdef _WIN32
    ULONG_PTR low = 0;
    ULONG_PTR high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    Require(low == reinterpret_cast<ULONG_PTR>(stack->bytes) && high == reinterpret_cast<ULONG_PTR>(stack->bytes + StackBytes), what);
#else
    (void)stack;
    (void)what;
#endif
}

void ThrowAndCatch() {
    bool caught = false;
    try {
        throw std::runtime_error("host exception on a context stack");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    Require(caught, "a host exception is caught on a context stack");
}

[[gnu::noinline]] void WalkContextChain() {
    auto* frame = static_cast<std::uintptr_t*>(__builtin_frame_address(0));
    const auto low = reinterpret_cast<std::uintptr_t>(g_contextStack.bytes);
    const auto high = low + StackBytes;
    for (int depth = 0; depth < 100; ++depth) {
        const auto address = reinterpret_cast<std::uintptr_t>(frame);
        if (address < low || address + 2 * sizeof(std::uintptr_t) > high) return;
        if (frame[1] == 0) {
            g_contextChainEnded = frame[0] == 0;
            return;
        }
        auto* next = reinterpret_cast<std::uintptr_t*>(frame[0]);
        if (reinterpret_cast<std::uintptr_t>(next) <= address) return;
        frame = next;
    }
}

[[gnu::noinline]] void GrowStack(int depth) {
    volatile unsigned char page[16 * 1024];
    page[0] = static_cast<unsigned char>(depth);
    page[sizeof(page) - 1] = static_cast<unsigned char>(depth);
    if (depth > 0) GrowStack(depth - 1);
    Require(page[0] == static_cast<unsigned char>(depth), "a deep host frame keeps its contents");
}

void CheckErrors() {
    *__error_nid_postfix() = 0;
    Require(getcontext_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == GuestEinval, "getcontext rejects a null context");
    *__error_nid_postfix() = 0;
    Require(setcontext_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == GuestEinval, "setcontext rejects a null context");
    UserUcontext context{};
    Require(getcontext_nid_postfix(&context) == 0, "capture a context to corrupt");
    context.mcontext.len = 0;
    *__error_nid_postfix() = 0;
    Require(setcontext_nid_postfix(&context) == -1 && *__error_nid_postfix() == GuestEinval, "setcontext rejects a wrong mc_len");
    context.mcontext.len = sizeof(UserMcontext);
    context.mcontext.flags = 0x8;
    *__error_nid_postfix() = 0;
    Require(setcontext_nid_postfix(&context) == -1 && *__error_nid_postfix() == GuestEinval, "setcontext rejects unknown mc_flags");
    context.mcontext.flags = 0;
    UserUcontext invalid = context;
    invalid.stackPointer = g_contextStack.bytes;
    invalid.stackSize = StackBytes;
    makecontext_nid_postfix(&invalid, &EntryThunk, 7);

    Require(invalid.mcontext.len == 0, "makecontext invalidates a context with more than six arguments");
    invalid = context;
    invalid.stackPointer = g_contextStack.bytes;
    invalid.stackSize = 2047;
    makecontext_nid_postfix(&invalid, &EntryThunk, 0);

    Require(invalid.mcontext.len == 0, "makecontext invalidates a context with a stack below MINSIGSTKSZ");
    invalid = context;
    invalid.stackPointer = nullptr;
    invalid.stackSize = StackBytes;
    makecontext_nid_postfix(&invalid, &EntryThunk, 0);

    Require(invalid.mcontext.len == 0, "makecontext invalidates a context without a stack");
    invalid = context;
    invalid.stackPointer = g_contextStack.bytes;
    invalid.stackSize = StackBytes;
    invalid.mcontext.len = 0;
    makecontext_nid_postfix(&invalid, &EntryThunk, 0);

    Require(invalid.mcontext.len == 0 && invalid.mcontext.rip == context.mcontext.rip, "makecontext leaves a context without a valid mc_len untouched");
    makecontext_nid_postfix(nullptr, &EntryThunk, 0);

}

void CheckCapture() {
    UserUcontext context;
    std::memset(&context, 0xa5, sizeof(context));
    UserUcontext untouched;
    std::memset(&untouched, 0xa5, sizeof(untouched));
    std::uint32_t previous[4] = {};
    const std::uint32_t blocked[4] = {0x24, 0, 0, 0};
    Require(sigprocmask_nid_postfix(GuestSigSetmask, blocked, previous) == 0, "block signals before the capture");
    Require(CaptureKnownRegisters(&context) == 0, "getcontext returns 0");
    Require(sigprocmask_nid_postfix(GuestSigSetmask, previous, nullptr) == 0, "restore the signal mask");
    const auto& machine = context.mcontext;
    Require(machine.rbx == 0x1111111111111111ull && machine.rbp == 0x2222222222222222ull, "getcontext saves rbx and rbp");
    Require(machine.r12 == 0x3333333333333333ull && machine.r13 == 0x4444444444444444ull, "getcontext saves r12 and r13");
    Require(machine.r14 == 0x5555555555555555ull && machine.r15 == 0x6666666666666666ull, "getcontext saves r14 and r15");
    Require(machine.rcx == 0x7777777777777777ull && machine.rsi == 0x8888888888888888ull, "getcontext saves rcx and rsi");
    Require(machine.r8 == 0x9999999999999999ull && machine.r9 == 0xaaaaaaaaaaaaaaaaull, "getcontext saves r8 and r9");
    Require(machine.r10 == 0xbbbbbbbbbbbbbbbbull && machine.r11 == 0xccccccccccccccccull, "getcontext saves r10 and r11");
    Require(machine.rdi == reinterpret_cast<std::uint64_t>(&context), "getcontext saves rdi");
    Require(machine.rax == 0 && machine.rdx == 0, "getcontext stores rax and rdx as 0");
    Require((machine.rflags & 1) == 0, "getcontext clears the carry flag");
    Require(machine.rip == reinterpret_cast<std::uint64_t>(&CaptureReturnAddress), "getcontext saves its return address");
    Require(machine.rsp == CapturedStackPointer, "getcontext saves the caller's stack pointer");
    Require(machine.len == sizeof(UserMcontext) && machine.fpformat == 0x10002 && machine.ownedfp == 0x20001, "getcontext sets mc_len, mc_fpformat and mc_ownedfp");
    Require(machine.onstack == 0 && machine.trapno == 0 && machine.addr == 0 && machine.flags == 0 && machine.err == 0, "getcontext zeroes the trap fields");
    Require(machine.fsbase == 0 && machine.gsbase == 0 && machine.xfpustate == 0 && machine.xfpustateLength == 0, "getcontext zeroes the bases and the extended FPU state");
    for (const auto word : machine.spare) Require(word == 0, "getcontext zeroes mc_spare");
    Require(context.sigmask[0] == 0x24 && context.sigmask[1] == 0 && context.sigmask[2] == 0 && context.sigmask[3] == 0, "getcontext saves the signal mask");
    std::uint32_t mxcsr = 0;
    std::memcpy(&mxcsr, reinterpret_cast<const std::uint8_t*>(machine.fpstate) + FxsaveMxcsr, sizeof(mxcsr));
    std::uint16_t control = 0;
    std::memcpy(&control, machine.fpstate, sizeof(control));
    Require(mxcsr == _mm_getcsr() && control == ControlWord(), "getcontext saves MXCSR and the x87 control word");
    Require(std::memcmp(&context.link, &untouched.link, sizeof(UserUcontext) - offsetof(UserUcontext, link)) == 0, "getcontext keeps uc_link, uc_stack and uc_flags");
}

void CheckRestore() {
    const auto mxcsrBefore = _mm_getcsr();
    const auto controlBefore = ControlWord();
    UserUcontext target{};
    volatile int phase = 0;
    Require(getcontext_nid_postfix(&ReturnContext) == 0, "capture the return context");
    if (phase == 0) {
        phase = 1;
        Require(getcontext_nid_postfix(&target) == 0, "capture the target template");
        auto& machine = target.mcontext;
        machine.rax = Pattern(0);
        machine.rbx = Pattern(1);
        machine.rcx = Pattern(2);
        machine.rdx = Pattern(3);
        machine.rsi = Pattern(4);
        machine.rdi = Pattern(5);
        machine.rbp = Pattern(6);
        machine.r8 = Pattern(8);
        machine.r9 = Pattern(9);
        machine.r10 = Pattern(10);
        machine.r11 = Pattern(11);
        machine.r12 = Pattern(12);
        machine.r13 = Pattern(13);
        machine.r14 = Pattern(14);
        machine.r15 = Pattern(15);
        machine.rsp = reinterpret_cast<std::uint64_t>(g_restoreStack.bytes + StackBytes - 256);
        machine.rip = reinterpret_cast<std::uint64_t>(&RecordRegisters);
        machine.rflags = (machine.rflags & ~UserChangeableFlags) | 0x8c1;
        auto* image = reinterpret_cast<std::uint8_t*>(machine.fpstate);
        const std::uint16_t control = 0x027f;
        std::memcpy(image, &control, sizeof(control));
        const std::uint32_t mxcsr = 0x80007f80u;
        std::memcpy(image + FxsaveMxcsr, &mxcsr, sizeof(mxcsr));
        for (unsigned index = 0; index < 32; ++index) {
            const auto value = Pattern(index + 16);
            std::memcpy(image + FxsaveXmm + index * 8, &value, sizeof(value));
        }
        setcontext_nid_postfix(&target);

        Require(false, "setcontext returned from a valid context");
    }
    const auto& machine = target.mcontext;
    Require(RecordedRegisters[0] == Pattern(0) && RecordedRegisters[1] == Pattern(1) && RecordedRegisters[2] == Pattern(2) && RecordedRegisters[3] == Pattern(3), "setcontext restores rax, rbx, rcx and rdx");
    Require(RecordedRegisters[4] == Pattern(4) && RecordedRegisters[5] == Pattern(5) && RecordedRegisters[6] == Pattern(6), "setcontext restores rsi, rdi and rbp");
    for (unsigned index = 8; index < 16; ++index) Require(RecordedRegisters[index] == Pattern(index), "setcontext restores r8 to r15");
    Require(RecordedRegisters[7] == machine.rsp, "setcontext restores rsp without touching the target stack");
    Require((RecordedRegisters[17] & UserChangeableFlags) == 0x8c1 && (RecordedRegisters[17] & InterruptFlag) != 0, "setcontext restores the user-changeable flags only");
    Require(RecordedMxcsr == 0x7f80u, "setcontext restores MXCSR without its reserved bits");
    Require(RecordedControlWord == 0x027f, "setcontext restores the x87 control word");
    for (unsigned index = 0; index < 32; ++index) {
        std::uint64_t value = 0;
        std::memcpy(&value, RecordedXmm + index * 8, sizeof(value));
        Require(value == Pattern(index + 16), "setcontext restores XMM0 to XMM15");
    }
    Require(_mm_getcsr() == mxcsrBefore && ControlWord() == controlBefore, "the return context restores MXCSR and the x87 control word");
}

}

extern "C" void APS5_VABI ContextBody(std::uint64_t first, std::uint64_t second, std::uint64_t third, std::uint64_t fourth, std::uint64_t fifth, std::uint64_t sixth) {
    g_arguments[0] = first;
    g_arguments[1] = second;
    g_arguments[2] = third;
    g_arguments[3] = fourth;
    g_arguments[4] = fifth;
    g_arguments[5] = sixth;
    CheckStackBounds(&g_contextStack, "the stack bounds follow setcontext onto the context stack");
    WalkContextChain();
    ThrowAndCatch();
    ++g_entries;
}

extern "C" void APS5_VABI ReturnWithoutLink() {}

namespace {

[[gnu::noinline]] void Switch(Fibre* from, Fibre* to) {
    from->started = 1;
    if (g_signalJumps) {
        if (sigsetjmp_nid_postfix(from->jump, 1) == 0) {
            if (to->started) siglongjmp_nid_postfix(to->jump, 1);
            setcontext_nid_postfix(&to->context);

            Require(false, "setcontext into the job returned");
        }
        return;
    }
    if (setjmp_nid_postfix(from->jump) == 0) {
        if (to->started) longjmp_nid_postfix(to->jump, 1);
        setcontext_nid_postfix(&to->context);

        Require(false, "setcontext into the job returned");
    }
}

}

extern "C" void APS5_VABI JobEntry() {
    for (;;) {
        ++g_jobRuns;
        CheckStackBounds(&g_jobStack, g_signalJumps ? "the stack bounds follow siglongjmp onto the job stack" : "the stack bounds follow longjmp onto the job stack");
        ThrowAndCatch();
        Switch(&g_job, &g_dispatcher);
    }
}

namespace {

void CheckMakecontext() {
#ifdef _WIN32
    ULONG_PTR threadLow = 0;
    ULONG_PTR threadHigh = 0;
    GetCurrentThreadStackLimits(&threadLow, &threadHigh);
#endif
    UserUcontext caller{};
    UserUcontext context{};
    volatile int phase = 0;
    Require(getcontext_nid_postfix(&caller) == 0, "capture the caller");
    if (phase == 0) {
        phase = 1;
        Require(getcontext_nid_postfix(&context) == 0, "capture the context template");
        context.stackPointer = g_contextStack.bytes;
        context.stackSize = StackBytes;
        context.link = &caller;
        makecontext_nid_postfix(&context, &EntryThunk, 6, 0x8000000000000001ull, 0x8000000000000002ull, 0x8000000000000003ull, 0x8000000000000004ull, 0x8000000000000005ull, 0x8000000000000006ull);

        Require(context.mcontext.len == sizeof(UserMcontext), "makecontext keeps the context valid");
        setcontext_nid_postfix(&context);

        Require(false, "setcontext into the made context returned");
    }
    Require(g_entries == 1, "the made context runs once and resumes uc_link");
    Require(g_contextChainEnded, "the frame chain of a made context ends on its stack at a frame whose saved frame pointer and return address are 0");
    for (unsigned index = 0; index < 6; ++index) Require(g_arguments[index] == 0x8000000000000001ull + index, "makecontext passes six 64-bit arguments");
    const auto stackLow = reinterpret_cast<std::uint64_t>(g_contextStack.bytes);
    Require(EntryStackPointer > stackLow && EntryStackPointer < stackLow + StackBytes, "the made context runs on its own stack");
    Require((EntryStackPointer + 8) % 16 == 0, "the made context's function starts with an aligned stack");
#ifdef _WIN32
    ULONG_PTR low = 0;
    ULONG_PTR high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    Require(low == threadLow && high == threadHigh, "the stack bounds follow setcontext back to the thread stack");
#endif
}

void CheckJumpBuffers(bool signalJumps) {
    g_signalJumps = signalJumps;
    g_jobRuns = 0;
    g_dispatcher = {};
    g_job = {};
#ifdef _WIN32
    ULONG_PTR threadLow = 0;
    ULONG_PTR threadHigh = 0;
    GetCurrentThreadStackLimits(&threadLow, &threadHigh);
#endif
    Require(getcontext_nid_postfix(&g_job.context) == 0, "capture the job template");
    g_job.context.stackPointer = g_jobStack.bytes;
    g_job.context.stackSize = StackBytes;
    g_job.context.link = nullptr;
    makecontext_nid_postfix(&g_job.context, &JobEntry, 0);

    for (int round = 1; round <= 4; ++round) {
        Switch(&g_dispatcher, &g_job);
        Require(g_jobRuns == round, "the job runs once per switch");
#ifdef _WIN32
        ULONG_PTR low = 0;
        ULONG_PTR high = 0;
        GetCurrentThreadStackLimits(&low, &high);
        Require(low == threadLow && high == threadHigh, signalJumps ? "the stack bounds follow siglongjmp back to the thread stack" : "the stack bounds follow longjmp back to the thread stack");
#endif
    }
}

void CheckSignalMask() {
    std::uint32_t original[4] = {};
    Require(sigprocmask_nid_postfix(GuestSigBlock, nullptr, original) == 0, "read the signal mask");
    const std::uint32_t first[4] = {0x10, 0, 0, 0};
    const std::uint32_t second[4] = {0x20, 0, 0, 0};
    UserUcontext context{};
    volatile int phase = 0;
    Require(sigprocmask_nid_postfix(GuestSigSetmask, first, nullptr) == 0, "set the first mask");
    Require(getcontext_nid_postfix(&context) == 0, "capture the masked context");
    if (phase == 0) {
        phase = 1;
        Require(sigprocmask_nid_postfix(GuestSigSetmask, second, nullptr) == 0, "set the second mask");
        setcontext_nid_postfix(&context);

        Require(false, "setcontext returned from a valid context");
    }
    std::uint32_t current[4] = {};
    Require(sigprocmask_nid_postfix(GuestSigBlock, nullptr, current) == 0 && current[0] == 0x10, "setcontext restores the context's signal mask");
    Require(sigprocmask_nid_postfix(GuestSigSetmask, original, nullptr) == 0, "restore the signal mask");
}

int ExitFromContext() {
    UserUcontext context{};
    Require(getcontext_nid_postfix(&context) == 0, "capture the exiting template");
    context.stackPointer = g_exitStack.bytes;
    context.stackSize = StackBytes;
    context.link = nullptr;
    makecontext_nid_postfix(&context, &ReturnWithoutLink, 0);
    setcontext_nid_postfix(&context);

    return 1;
}

}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "exit-on-return") == 0) return ExitFromContext();
    CheckErrors();
    CheckCapture();
    CheckRestore();
    CheckSignalMask();
    std::thread([] {
        CheckMakecontext();
        CheckJumpBuffers(false);
        CheckJumpBuffers(true);
        GrowStack(32);
    }).join();
    return 0;
}
