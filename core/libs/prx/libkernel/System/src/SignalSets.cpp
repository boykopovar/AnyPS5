#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

static_assert(sizeof(GuestSignalSet) == 16);

namespace {
constexpr int MaximumSignal = 128;
constexpr int InvalidArgument = 22;

bool ValidSignal(int signal) { return signal > 0 && signal <= MaximumSignal; }
std::size_t WordIndex(int signal) { return static_cast<std::size_t>(signal - 1) >> 5; }
std::uint32_t Bit(int signal) { return 1u << ((signal - 1) & 31); }
int RejectSignal() {
    *__error_nid_postfix() = InvalidArgument;
    return -1;
}
void RequireSet(const GuestSignalSet* set, const char* function) {
    if (set == nullptr) throw std::invalid_argument(std::string(function) + ": set is null");
}
}

extern "C" {
int APS5_VABI sigemptyset_nid_postfix(GuestSignalSet* set) {
    RequireSet(set, "sigemptyset");
    for (auto& word : set->bits) word = 0;
    return 0;
}

int APS5_VABI sigfillset_nid_postfix(GuestSignalSet* set) {
    RequireSet(set, "sigfillset");
    for (auto& word : set->bits) word = ~0u;
    return 0;
}

int APS5_VABI sigaddset_nid_postfix(GuestSignalSet* set, int signal) {
    RequireSet(set, "sigaddset");
    if (!ValidSignal(signal)) return RejectSignal();
    set->bits[WordIndex(signal)] |= Bit(signal);
    return 0;
}

int APS5_VABI sigdelset_nid_postfix(GuestSignalSet* set, int signal) {
    RequireSet(set, "sigdelset");
    if (!ValidSignal(signal)) return RejectSignal();
    set->bits[WordIndex(signal)] &= ~Bit(signal);
    return 0;
}

int APS5_VABI sigismember_nid_postfix(const GuestSignalSet* set, int signal) {
    RequireSet(set, "sigismember");
    if (!ValidSignal(signal)) return RejectSignal();
    return (set->bits[WordIndex(signal)] & Bit(signal)) != 0 ? 1 : 0;
}
}
#define APS5_SIGNAL_ASM_FUNCTION(name) APS5_ASM_FUNCTION(name)
#if defined(_WIN32) || defined(__APPLE__)
#define APS5_SIGNAL_ASM_CALL(name) "    call " APS5_ASM_SYMBOL(name) "\n"
#else
#define APS5_SIGNAL_ASM_CALL(name) "    call " name "@PLT\n"
#endif

asm(".text\n"
    APS5_SIGNAL_ASM_FUNCTION("sigsetjmp_nid_postfix")
    "    movl %esi, 88(%rdi)\n"
    "    testl %esi, %esi\n"
    "    jz 2f\n"
    "    pushq %rdi\n"
    "    movq %rdi, %rcx\n"
    "    movl $1, %edi\n"
    "    xorl %esi, %esi\n"
    "    leaq 72(%rcx), %rdx\n"
    APS5_SIGNAL_ASM_CALL("_sigprocmask_nid_postfix")
    "    popq %rdi\n"
    "2:\n"
    "    movq %rdi, %rcx\n"
    "    movq 0(%rsp), %rdx\n"
    "    movq %rdx, 0(%rcx)\n"
    "    movq %rbx, 8(%rcx)\n"
    "    movq %rsp, 16(%rcx)\n"
    "    movq %rbp, 24(%rcx)\n"
    "    movq %r12, 32(%rcx)\n"
    "    movq %r13, 40(%rcx)\n"
    "    movq %r14, 48(%rcx)\n"
    "    movq %r15, 56(%rcx)\n"
    "    fnstcw 64(%rcx)\n"
    "    xorl %eax, %eax\n"
    "    ret\n"
    APS5_SIGNAL_ASM_FUNCTION("siglongjmp_nid_postfix")
    "    cmpl $0, 88(%rdi)\n"
    "    jz 2f\n"
    "    movq %rdi, %rdx\n"
    "    pushq %rdi\n"
    "    pushq %rsi\n"
    "    movl $3, %edi\n"
    "    leaq 72(%rdx), %rsi\n"
    "    xorl %edx, %edx\n"
    "    subq $8, %rsp\n"
    APS5_SIGNAL_ASM_CALL("_sigprocmask_nid_postfix")
    "    addq $8, %rsp\n"
    "    popq %rsi\n"
    "    popq %rdi\n"
    "2:\n"
#ifdef _WIN32
    "    pushq %rdi\n"
    "    pushq %rsi\n"
    "    movq 16(%rdi), %rdi\n"
    "    subq $8, %rsp\n"
    "    call GuestStackSwitch_nid_no_patch\n"
    "    addq $8, %rsp\n"
    "    popq %rsi\n"
    "    popq %rdi\n"
#endif
    "    movq %rdi, %rdx\n"
    "    movl %esi, %eax\n"
    "    movq 0(%rdx), %rcx\n"
    "    movq 8(%rdx), %rbx\n"
    "    movq 16(%rdx), %rsp\n"
    "    movq 24(%rdx), %rbp\n"
    "    movq 32(%rdx), %r12\n"
    "    movq 40(%rdx), %r13\n"
    "    movq 48(%rdx), %r14\n"
    "    movq 56(%rdx), %r15\n"
    "    fldcw 64(%rdx)\n"
    "    testl %eax, %eax\n"
    "    jnz 1f\n"
    "    incl %eax\n"
    "1:\n"
    "    movq %rcx, 0(%rsp)\n"
    "    ret\n");
