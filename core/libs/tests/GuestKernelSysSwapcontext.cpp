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
#include <source_location>

using ContextEntry = void (APS5_VABI*)();

extern "C" {
int APS5_VABI SwapKeepingRegisters(UserUcontext* saved, const UserUcontext* next);
int APS5_VABI SwapClobberingRegisters(UserUcontext* saved, const UserUcontext* next);
}

asm(".text\n"
    APS5_ASM_FUNCTION("SwapKeepingRegisters")
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
    "    call " APS5_ASM_SYMBOL("swapcontext_nid_postfix") "\n"
    "    movabsq $0x1111111111111111, %rcx\n"
    "    cmp %rcx, %rbx\n"
    "    jne 1f\n"
    "    movabsq $0x2222222222222222, %rcx\n"
    "    cmp %rcx, %rbp\n"
    "    jne 1f\n"
    "    movabsq $0x3333333333333333, %rcx\n"
    "    cmp %rcx, %r12\n"
    "    jne 1f\n"
    "    movabsq $0x4444444444444444, %rcx\n"
    "    cmp %rcx, %r13\n"
    "    jne 1f\n"
    "    movabsq $0x5555555555555555, %rcx\n"
    "    cmp %rcx, %r14\n"
    "    jne 1f\n"
    "    movabsq $0x6666666666666666, %rcx\n"
    "    cmp %rcx, %r15\n"
    "    je 2f\n"
    "1:\n"
    "    mov $-100, %eax\n"
    "2:\n"
    "    add $8, %rsp\n"
    "    pop %r15\n"
    "    pop %r14\n"
    "    pop %r13\n"
    "    pop %r12\n"
    "    pop %rbp\n"
    "    pop %rbx\n"
    "    ret\n"
    APS5_ASM_FUNCTION("SwapClobberingRegisters")
    "    push %rbx\n"
    "    push %rbp\n"
    "    push %r12\n"
    "    push %r13\n"
    "    push %r14\n"
    "    push %r15\n"
    "    sub $8, %rsp\n"
    "    movabsq $0x0badc0de0badc0de, %rbx\n"
    "    mov %rbx, %rbp\n"
    "    mov %rbx, %r12\n"
    "    mov %rbx, %r13\n"
    "    mov %rbx, %r14\n"
    "    mov %rbx, %r15\n"
    "    call " APS5_ASM_SYMBOL("swapcontext_nid_postfix") "\n"
    "    add $8, %rsp\n"
    "    pop %r15\n"
    "    pop %r14\n"
    "    pop %r13\n"
    "    pop %r12\n"
    "    pop %rbp\n"
    "    pop %rbx\n"
    "    ret\n");

namespace {

constexpr int Einval = 22;
constexpr int SigBlock = 1;
constexpr int SigSetmask = 3;
constexpr int SigUsr1 = 30;
constexpr std::size_t StackBytes = 256 * 1024;

struct alignas(16) Stack {
    unsigned char bytes[StackBytes];
};

Stack g_stack;
UserUcontext g_main = {};
UserUcontext g_coroutine = {};
int g_steps = 0;
bool g_coroutineSawBlocked = true;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

bool Usr1Blocked() {
    std::uint32_t mask[4] = {};
    Require(sigprocmask_nid_postfix(SigBlock, nullptr, mask) == 0);
    return (mask[0] & (1u << (SigUsr1 - 1))) != 0;
}

void APS5_VABI Coroutine() {
    g_coroutineSawBlocked = Usr1Blocked();
    for (int step = 0; step < 3; ++step) {
        ++g_steps;
        Require(SwapClobberingRegisters(&g_coroutine, &g_main) == 0);
    }
}

void Arguments() {
    UserUcontext context = {};
    Require(getcontext_nid_postfix(&context) == 0);
    Require(swapcontext_nid_postfix(nullptr, &context) == -1 && Error() == Einval);
    Require(swapcontext_nid_postfix(&context, nullptr) == -1 && Error() == Einval);

    UserUcontext saved = {};
    UserUcontext invalid = context;
    invalid.mcontext.len = 0;
    Require(swapcontext_nid_postfix(&saved, &invalid) == -1 && Error() == Einval);
    Require(saved.mcontext.len == sizeof(UserMcontext) && saved.mcontext.rip != 0);
}

void PingPong() {
    Require(getcontext_nid_postfix(&g_coroutine) == 0);
    g_coroutine.stackPointer = g_stack.bytes;
    g_coroutine.stackSize = sizeof(g_stack.bytes);
    g_coroutine.link = &g_main;
    makecontext_nid_postfix(&g_coroutine, &Coroutine, 0);
    Require(g_coroutine.mcontext.len == sizeof(UserMcontext));

    std::uint32_t block[4] = {1u << (SigUsr1 - 1), 0, 0, 0};
    std::uint32_t previous[4] = {};
    Require(sigprocmask_nid_postfix(SigBlock, block, previous) == 0);
    for (int expected = 1; expected <= 3; ++expected) {
        Require(SwapKeepingRegisters(&g_main, &g_coroutine) == 0);
        Require(g_steps == expected && Usr1Blocked());
    }
    Require(!g_coroutineSawBlocked);
    Require(SwapKeepingRegisters(&g_main, &g_coroutine) == 0);
    Require(g_steps == 3 && Usr1Blocked());
    Require(sigprocmask_nid_postfix(SigSetmask, previous, nullptr) == 0);
}

}

int main() {
    Arguments();
    PingPong();
    std::puts("guest_kernel_sys_swapcontext passed");
    return 0;
}
