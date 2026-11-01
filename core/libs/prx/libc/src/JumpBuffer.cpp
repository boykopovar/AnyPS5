#include "prx/libc/include/JumpBuffer.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace JumpBuffer {
namespace {

constexpr std::size_t MaskOffset = 72;
constexpr std::size_t MaskSavedOffset = 88;
constexpr int SigBlock = 1;
constexpr int SigSetmask = 3;

std::atomic<Sigprocmask> guestSigprocmask{nullptr};

}

extern "C" {

void GuestSigprocmaskRegister_nid_no_patch(Sigprocmask sigprocmask) {
    guestSigprocmask.store(sigprocmask);
}

void APS5_VABI JumpBufferSaveMask_nid_no_patch(unsigned char* buffer) {
    const auto sigprocmask = guestSigprocmask.load();
    const std::int32_t saved = sigprocmask != nullptr && sigprocmask(SigBlock, nullptr, buffer + MaskOffset) == 0;
    std::memcpy(buffer + MaskSavedOffset, &saved, sizeof(saved));
}

void APS5_VABI JumpBufferRestoreMask_nid_no_patch(const unsigned char* buffer) {
    std::int32_t saved = 0;
    std::memcpy(&saved, buffer + MaskSavedOffset, sizeof(saved));
    const auto sigprocmask = guestSigprocmask.load();
    if (saved != 0 && sigprocmask != nullptr) sigprocmask(SigSetmask, buffer + MaskOffset, nullptr);
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
    "    jmp *0(%rdi)\n");
