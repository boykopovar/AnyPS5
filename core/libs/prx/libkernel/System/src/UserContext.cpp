#include "prx/libc/include/GuestStacks.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/System/include/GuestContext.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <immintrin.h>

extern "C" int* APS5_VABI __error_nid_postfix();
extern "C" int APS5_VABI sigprocmask_nid_postfix(int how, const void* set, void* previousSet);

namespace {

struct RegisterFrame {
    std::uint64_t rax;
    std::uint64_t rbx;
    std::uint64_t rcx;
    std::uint64_t rdx;
    std::uint64_t rsi;
    std::uint64_t rdi;
    std::uint64_t rbp;
    std::uint64_t rsp;
    std::uint64_t r8;
    std::uint64_t r9;
    std::uint64_t r10;
    std::uint64_t r11;
    std::uint64_t r12;
    std::uint64_t r13;
    std::uint64_t r14;
    std::uint64_t r15;
    std::uint64_t rip;
    std::uint64_t rflags;
    std::uint64_t cs;
    std::uint64_t ss;
    alignas(16) std::uint8_t fxsave[512];
};

static_assert(offsetof(RegisterFrame, rsp) == 0x38 && offsetof(RegisterFrame, r8) == 0x40 && offsetof(RegisterFrame, r12) == 0x60 && offsetof(RegisterFrame, r15) == 0x78);
static_assert(offsetof(RegisterFrame, rip) == 0x80 && offsetof(RegisterFrame, rflags) == 0x88 && offsetof(RegisterFrame, cs) == 0x90 && offsetof(RegisterFrame, ss) == 0x98);
static_assert(offsetof(RegisterFrame, fxsave) == 0xa0 && sizeof(RegisterFrame) == 0x2a0);

constexpr int GuestEinval = 22;
constexpr int GuestSigBlock = 1;
constexpr int GuestSigSetmask = 3;
constexpr std::uint64_t CarryFlag = 0x1;
constexpr std::uint32_t McontextFlagMask = 0x7;
constexpr std::uint64_t FpFormatXmm = 0x10002;
constexpr std::uint64_t FpOwnedFpu = 0x20001;
constexpr std::size_t FxsaveMxcsr = 24;
constexpr std::size_t FxsaveMxcsrMask = 28;
constexpr std::uint32_t DefaultMxcsrMask = 0xffbf;

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

std::uint32_t HostMxcsrMask() {
    static const std::uint32_t mask = [] {
        alignas(16) std::uint8_t image[512]{};
        _fxsave64(image);
        std::uint32_t value = 0;
        std::memcpy(&value, image + FxsaveMxcsrMask, sizeof(value));
        return value != 0 ? value : DefaultMxcsrMask;
    }();
    return mask;
}

}

extern "C" [[noreturn]] void APS5_VABI Aps5ContextRestore_nid_no_patch(const RegisterFrame* frame);

asm(".text\n"
    APS5_ASM_FUNCTION("getcontext_nid_postfix")
    "    pushfq\n"
    "    push %rbp\n"
    "    mov %rsp, %rbp\n"
    "    sub $0x2a0, %rsp\n"
    "    and $-16, %rsp\n"
    "    mov %rax, 0x00(%rsp)\n"
    "    mov %rbx, 0x08(%rsp)\n"
    "    mov %rcx, 0x10(%rsp)\n"
    "    mov %rdx, 0x18(%rsp)\n"
    "    mov %rsi, 0x20(%rsp)\n"
    "    mov %rdi, 0x28(%rsp)\n"
    "    mov 0(%rbp), %rax\n"
    "    mov %rax, 0x30(%rsp)\n"
    "    lea 24(%rbp), %rax\n"
    "    mov %rax, 0x38(%rsp)\n"
    "    mov %r8, 0x40(%rsp)\n"
    "    mov %r9, 0x48(%rsp)\n"
    "    mov %r10, 0x50(%rsp)\n"
    "    mov %r11, 0x58(%rsp)\n"
    "    mov %r12, 0x60(%rsp)\n"
    "    mov %r13, 0x68(%rsp)\n"
    "    mov %r14, 0x70(%rsp)\n"
    "    mov %r15, 0x78(%rsp)\n"
    "    mov 16(%rbp), %rax\n"
    "    mov %rax, 0x80(%rsp)\n"
    "    mov 8(%rbp), %rax\n"
    "    mov %rax, 0x88(%rsp)\n"
    "    xor %eax, %eax\n"
    "    mov %cs, %ax\n"
    "    mov %rax, 0x90(%rsp)\n"
    "    mov %ss, %ax\n"
    "    mov %rax, 0x98(%rsp)\n"
    "    fxsave64 0xa0(%rsp)\n"
    "    mov %rsp, %rsi\n"
    "    call Aps5GetcontextFinish_nid_no_patch\n"
    "    mov %rbp, %rsp\n"
    "    pop %rbp\n"
    "    lea 8(%rsp), %rsp\n"
    "    ret\n"
    APS5_ASM_FUNCTION("Aps5ContextRestore_nid_no_patch")
    "    mov %rdi, %r12\n"
    "    xor %eax, %eax\n"
    "    mov %ss, %ax\n"
    "    push %rax\n"
    "    pushq 0x38(%r12)\n"
    "    pushfq\n"
    "    pop %rax\n"
    "    and $-0x254dd6, %rax\n"
    "    mov 0x88(%r12), %rcx\n"
    "    and $0x254dd5, %rcx\n"
    "    or %rcx, %rax\n"
    "    push %rax\n"
    "    xor %eax, %eax\n"
    "    mov %cs, %ax\n"
    "    push %rax\n"
    "    pushq 0x80(%r12)\n"
#ifdef _WIN32
    "    mov 0x38(%r12), %rdi\n"
    "    call GuestStackSwitch_nid_no_patch\n"
#endif
    "    fxrstor64 0xa0(%r12)\n"
    "    mov 0x00(%r12), %rax\n"
    "    mov 0x08(%r12), %rbx\n"
    "    mov 0x10(%r12), %rcx\n"
    "    mov 0x18(%r12), %rdx\n"
    "    mov 0x20(%r12), %rsi\n"
    "    mov 0x28(%r12), %rdi\n"
    "    mov 0x30(%r12), %rbp\n"
    "    mov 0x40(%r12), %r8\n"
    "    mov 0x48(%r12), %r9\n"
    "    mov 0x50(%r12), %r10\n"
    "    mov 0x58(%r12), %r11\n"
    "    mov 0x68(%r12), %r13\n"
    "    mov 0x70(%r12), %r14\n"
    "    mov 0x78(%r12), %r15\n"
    "    mov 0x60(%r12), %r12\n"
    "    iretq\n");

extern "C" {

int APS5_VABI Aps5GetcontextFinish_nid_no_patch(GuestUcontext* context, const RegisterFrame* frame) {
    if (context == nullptr) return Fail(GuestEinval);
    GuestUcontext captured{};
    sigprocmask_nid_postfix(GuestSigBlock, nullptr, captured.sigmask);
    auto& machine = captured.mcontext;
    machine.rdi = frame->rdi;
    machine.rsi = frame->rsi;
    machine.rcx = frame->rcx;
    machine.r8 = frame->r8;
    machine.r9 = frame->r9;
    machine.rbx = frame->rbx;
    machine.rbp = frame->rbp;
    machine.r10 = frame->r10;
    machine.r11 = frame->r11;
    machine.r12 = frame->r12;
    machine.r13 = frame->r13;
    machine.r14 = frame->r14;
    machine.r15 = frame->r15;
    machine.rip = frame->rip;
    machine.cs = frame->cs;
    machine.rflags = frame->rflags & ~CarryFlag;
    machine.rsp = frame->rsp;
    machine.ss = frame->ss;
    machine.len = sizeof(GuestMcontext);
    machine.fpformat = FpFormatXmm;
    machine.ownedfp = FpOwnedFpu;
    std::memcpy(machine.fpstate, frame->fxsave, sizeof(frame->fxsave));
    std::memcpy(context, &captured, offsetof(GuestUcontext, link));
    return 0;
}

int APS5_VABI setcontext_nid_postfix(const GuestUcontext* context) {
    if (context == nullptr) return Fail(GuestEinval);
    const auto& machine = context->mcontext;
    if (machine.len != sizeof(GuestMcontext) || (machine.flags & ~McontextFlagMask) != 0) return Fail(GuestEinval);
    RegisterFrame frame{};
    frame.rax = machine.rax;
    frame.rbx = machine.rbx;
    frame.rcx = machine.rcx;
    frame.rdx = machine.rdx;
    frame.rsi = machine.rsi;
    frame.rdi = machine.rdi;
    frame.rbp = machine.rbp;
    frame.rsp = machine.rsp;
    frame.r8 = machine.r8;
    frame.r9 = machine.r9;
    frame.r10 = machine.r10;
    frame.r11 = machine.r11;
    frame.r12 = machine.r12;
    frame.r13 = machine.r13;
    frame.r14 = machine.r14;
    frame.r15 = machine.r15;
    frame.rip = machine.rip;
    frame.rflags = machine.rflags;
    std::memcpy(frame.fxsave, machine.fpstate, sizeof(frame.fxsave));
    std::uint32_t mxcsr = 0;
    std::memcpy(&mxcsr, frame.fxsave + FxsaveMxcsr, sizeof(mxcsr));
    mxcsr &= HostMxcsrMask();
    std::memcpy(frame.fxsave + FxsaveMxcsr, &mxcsr, sizeof(mxcsr));
    sigprocmask_nid_postfix(GuestSigSetmask, context->sigmask, nullptr);
    Aps5ContextRestore_nid_no_patch(&frame);
}

}
