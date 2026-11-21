#include "prx/libc/include/JumpBuffer.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace JumpBuffer {
namespace {

constexpr std::size_t MaskOffset = 72;
constexpr std::size_t MaskSavedOffset = 88;
constexpr int SigBlock = 1;
constexpr int SigSetmask = 3;
constexpr std::uint32_t ContextSignature = 0x43545835;
constexpr std::size_t ContextOffset = 92;

std::atomic<Sigprocmask> guestSigprocmask{nullptr};
std::atomic<StackRestore> ioStackRestore{nullptr};
std::atomic<std::uint32_t> nextContext{1};
constinit thread_local std::uint32_t currentContext = 0;

std::uint32_t NewContext() {
    auto value = nextContext.load(std::memory_order_relaxed);
    for (;;) {
        if (value == std::numeric_limits<std::uint32_t>::max()) throw std::overflow_error("Guest context identities exhausted");
        if (nextContext.compare_exchange_weak(value, value + 1, std::memory_order_relaxed)) return value;
    }
}

}

extern "C" {

void GuestSigprocmaskRegister_nid_no_patch(Sigprocmask sigprocmask) {
    guestSigprocmask.store(sigprocmask);
}

void GuestIoJumpRegister_nid_no_patch(StackRestore restore) { ioStackRestore.store(restore); }

std::uint32_t GuestContextCurrent_nid_no_patch() {
    if (!currentContext) currentContext = NewContext();
    return currentContext;
}

void GuestContextSet_nid_no_patch(std::uint32_t context) { currentContext = context; }

Context GuestContextCapture_nid_no_patch(std::uintptr_t stack) {
    const auto identity = GuestContextCurrent_nid_no_patch();
    return {stack, identity, ContextSignature ^ identity};
}

Context GuestContextCreate_nid_no_patch(std::uintptr_t stack) {
    const auto identity = NewContext();
    return {stack, identity, ContextSignature ^ identity};
}

std::uint32_t GuestContextIdentify_nid_no_patch(const Context& context, std::uintptr_t stack) {
    if (context.stack == stack && context.identity != 0 && context.identity < nextContext.load(std::memory_order_relaxed) &&
        context.signature == (ContextSignature ^ context.identity)) return context.identity;
    return NewContext();
}

void APS5_VABI JumpBufferSaveContext_nid_no_patch(unsigned char* buffer) {
    const auto context = GuestContextCurrent_nid_no_patch();
    std::memcpy(buffer + ContextOffset, &context, sizeof(context));
}

void APS5_VABI GuestIoStackRestore_nid_no_patch(std::uintptr_t stack, std::uint32_t context) {
    if (const auto restore = ioStackRestore.load()) restore(stack, context);
    else GuestContextSet_nid_no_patch(context);
}

void APS5_VABI JumpBufferSaveMask_nid_no_patch(unsigned char* buffer) {
    const auto sigprocmask = guestSigprocmask.load();
    const std::int32_t saved = sigprocmask != nullptr && sigprocmask(SigBlock, nullptr, buffer + MaskOffset) == 0;
    std::memcpy(buffer + MaskSavedOffset, &saved, sizeof(saved));
    JumpBufferSaveContext_nid_no_patch(buffer);
}

void APS5_VABI JumpBufferRestoreMask_nid_no_patch(const unsigned char* buffer) {
    std::uintptr_t stack = 0;
    std::memcpy(&stack, buffer + 16, sizeof(stack));
    std::int32_t saved = 0;
    std::memcpy(&saved, buffer + MaskSavedOffset, sizeof(saved));
    const auto sigprocmask = guestSigprocmask.load();
    if (saved != 0 && sigprocmask != nullptr) sigprocmask(SigSetmask, buffer + MaskOffset, nullptr);
    std::uint32_t context = 0;
    std::memcpy(&context, buffer + ContextOffset, sizeof(context));
    GuestIoStackRestore_nid_no_patch(stack, context);
}

}

}

// setjmp/longjmp must capture the guest's own frame, so they are written directly in assembly with
// the guest (System V) calling convention. The saved state fits the guest's 96-byte jmp_buf:
// return address, rbx, rsp, rbp, r12-r15, MXCSR, the x87 control word and, as FreeBSD's do, the
// signal mask at 72 with the flag sigsetjmp keeps at 88, once libkernel has registered sigprocmask.

#if defined(_WIN32) || defined(__APPLE__)
#define APS5_JUMP_ASM_CALL(name) "    call " APS5_ASM_SYMBOL(name) "\n"
#else
#define APS5_JUMP_ASM_CALL(name) "    call " name "@PLT\n"
#endif

asm(".text\n"
    APS5_ASM_FUNCTION("setjmp_nid_postfix")
    "    push %rdi\n"
    APS5_JUMP_ASM_CALL("JumpBufferSaveMask_nid_no_patch")
    "    pop %rdi\n"
    "    mov (%rsp), %rax\n"
    "    mov %rax, 0(%rdi)\n"
    "    mov %rbx, 8(%rdi)\n"
    "    lea 8(%rsp), %rax\n"
    "    mov %rax, 16(%rdi)\n"
    "    mov %rbp, 24(%rdi)\n"
    "    mov %r12, 32(%rdi)\n"
    "    mov %r13, 40(%rdi)\n"
    "    mov %r14, 48(%rdi)\n"
    "    mov %r15, 56(%rdi)\n"
    "    stmxcsr 64(%rdi)\n"
    "    fnstcw 68(%rdi)\n"
    "    xor %eax, %eax\n"
    "    ret\n"
    APS5_ASM_FUNCTION("longjmp_nid_postfix")
    "    mov %rdi, %rbx\n"
    "    mov %esi, %r12d\n"
    "    and $-16, %rsp\n"
    APS5_JUMP_ASM_CALL("JumpBufferRestoreMask_nid_no_patch")
#ifdef _WIN32
    "    mov 16(%rbx), %rdi\n"
    "    call GuestStackSwitch_nid_no_patch\n"
#endif
    "    mov %rbx, %rdi\n"
    "    mov %r12d, %esi\n"
    "    mov %esi, %eax\n"
    "    test %eax, %eax\n"
    "    jnz 1f\n"
    "    inc %eax\n"
    "1:\n"
    "    mov 8(%rdi), %rbx\n"
    "    mov 16(%rdi), %rsp\n"
    "    mov 24(%rdi), %rbp\n"
    "    mov 32(%rdi), %r12\n"
    "    mov 40(%rdi), %r13\n"
    "    mov 48(%rdi), %r14\n"
    "    mov 56(%rdi), %r15\n"
    "    ldmxcsr 64(%rdi)\n"
    "    fldcw 68(%rdi)\n"
    "    jmp *0(%rdi)\n"
    APS5_ASM_FUNCTION("_setjmp_nid_postfix")
    "    push %rdi\n"
    APS5_JUMP_ASM_CALL("JumpBufferSaveContext_nid_no_patch")
    "    pop %rdi\n"
    "    movl $0, 88(%rdi)\n"
    "    mov (%rsp), %rax\n"
    "    mov %rax, 0(%rdi)\n"
    "    mov %rbx, 8(%rdi)\n"
    "    lea 8(%rsp), %rax\n"
    "    mov %rax, 16(%rdi)\n"
    "    mov %rbp, 24(%rdi)\n"
    "    mov %r12, 32(%rdi)\n"
    "    mov %r13, 40(%rdi)\n"
    "    mov %r14, 48(%rdi)\n"
    "    mov %r15, 56(%rdi)\n"
    "    stmxcsr 64(%rdi)\n"
    "    fnstcw 68(%rdi)\n"
    "    xor %eax, %eax\n"
    "    ret\n"
    APS5_ASM_FUNCTION("_longjmp_nid_postfix")
    "    mov %rdi, %rbx\n"
    "    mov %esi, %r12d\n"
    "    and $-16, %rsp\n"
    "    mov 16(%rbx), %rdi\n"
    "    mov 92(%rbx), %esi\n"
    APS5_JUMP_ASM_CALL("GuestIoStackRestore_nid_no_patch")
#ifdef _WIN32
    "    mov 16(%rbx), %rdi\n"
    "    call GuestStackSwitch_nid_no_patch\n"
#endif
    "    mov %rbx, %rdi\n"
    "    mov %r12d, %esi\n"
    "    sub $8, %rsp\n"
    "    stmxcsr (%rsp)\n"
    "    mov 64(%rdi), %eax\n"
    "    and $-64, %eax\n"
    "    mov (%rsp), %edx\n"
    "    and $63, %edx\n"
    "    or %edx, %eax\n"
    "    mov %eax, (%rsp)\n"
    "    ldmxcsr (%rsp)\n"
    "    mov %esi, %eax\n"
    "    test %eax, %eax\n"
    "    jnz 1f\n"
    "    inc %eax\n"
    "1:\n"
    "    mov 8(%rdi), %rbx\n"
    "    mov 16(%rdi), %rsp\n"
    "    mov 24(%rdi), %rbp\n"
    "    mov 32(%rdi), %r12\n"
    "    mov 40(%rdi), %r13\n"
    "    mov 48(%rdi), %r14\n"
    "    mov 56(%rdi), %r15\n"
    "    fldcw 68(%rdi)\n"
    "    jmp *0(%rdi)\n");
