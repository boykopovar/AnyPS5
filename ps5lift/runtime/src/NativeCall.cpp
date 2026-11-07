#include "Runtime.hpp"

#include <ps5lift/Bridge.hpp>
#include <ps5lift/State.h>

#include <signal.h>
#include <ucontext.h>

#include <cerrno>
#include <cinttypes>
#include <cstdint>
#include <cstring>

namespace {

struct NativeRegisters {
    std::uint64_t Integer[8];
    unsigned char Vector[8][16];
};

static_assert(sizeof(NativeRegisters) == 192);

constexpr int GuestIntegerArguments = 6;
constexpr int GuestVectorArguments = 8;
constexpr int StackedIntegerArguments = 2;

struct sigaction PreviousSegv;
struct sigaction PreviousBus;

bool InsideGuestImage(const std::uint64_t address) {
    const auto& program = Ps5Lift::Program();
    for (std::uint64_t image = 0; image < program.ImageCount; ++image) {
        const auto& entry = program.Images[image];
        for (std::uint64_t segment = 0; segment < entry.SegmentCount; ++segment) {
            const auto& mapped = entry.Segments[segment];
            if (address - mapped.GuestAddress < mapped.MemorySize) return true;
        }
    }
    return false;
}

}

extern "C" void ps5lift_native_call(const std::uint64_t target, NativeRegisters* const registers) {
    using namespace Ps5Lift::Bridge;
    const GuestEntry entry;
    auto* state = entry.State();
    for (int index = 0; index < GuestIntegerArguments; ++index) state->Gpr[IntegerRegisters[index]] = registers->Integer[index];
    for (int index = 0; index < GuestVectorArguments; ++index) {
        std::memset(state->Vector[index], 0, sizeof(state->Vector[index]));
        std::memcpy(state->Vector[index], registers->Vector[index], sizeof(registers->Vector[index]));
    }
    RunGuest(state, target, registers->Integer + GuestIntegerArguments, StackedIntegerArguments, GuestVectorArguments);
    registers->Integer[0] = state->Gpr[PS5LIFT_RAX];
    registers->Integer[1] = state->Gpr[PS5LIFT_RDX];
    std::memcpy(registers->Vector[0], state->Vector[0], sizeof(registers->Vector[0]));
    std::memcpy(registers->Vector[1], state->Vector[1], sizeof(registers->Vector[1]));
}

extern "C" void ps5lift_native_call_trampoline();

#if defined(__aarch64__)
__asm__(".text\n"
        ".p2align 2\n"
        ".type ps5lift_native_call_trampoline, %function\n"
        "ps5lift_native_call_trampoline:\n"
        "    .cfi_startproc\n"
        "    stp x29, x30, [sp, #-16]!\n"
        "    .cfi_def_cfa_offset 16\n"
        "    .cfi_offset x29, -16\n"
        "    .cfi_offset x30, -8\n"
        "    mov x29, sp\n"
        "    .cfi_def_cfa_register x29\n"
        "    sub sp, sp, #192\n"
        "    stp x0, x1, [sp, #0]\n"
        "    stp x2, x3, [sp, #16]\n"
        "    stp x4, x5, [sp, #32]\n"
        "    stp x6, x7, [sp, #48]\n"
        "    stp q0, q1, [sp, #64]\n"
        "    stp q2, q3, [sp, #96]\n"
        "    stp q4, q5, [sp, #128]\n"
        "    stp q6, q7, [sp, #160]\n"
        "    mov x0, x16\n"
        "    mov x1, sp\n"
        "    bl ps5lift_native_call\n"
        "    ldp x0, x1, [sp, #0]\n"
        "    ldp q0, q1, [sp, #64]\n"
        "    mov sp, x29\n"
        "    .cfi_def_cfa_register sp\n"
        "    ldp x29, x30, [sp], #16\n"
        "    .cfi_def_cfa_offset 0\n"
        "    .cfi_restore x29\n"
        "    .cfi_restore x30\n"
        "    ret\n"
        "    .cfi_endproc\n"
        ".size ps5lift_native_call_trampoline, . - ps5lift_native_call_trampoline\n");
#elif defined(__x86_64__)
__asm__(".text\n"
        ".p2align 4\n"
        ".type ps5lift_native_call_trampoline, @function\n"
        "ps5lift_native_call_trampoline:\n"
        "    .cfi_startproc\n"
        "    push %rbp\n"
        "    .cfi_def_cfa_offset 16\n"
        "    .cfi_offset %rbp, -16\n"
        "    mov %rsp, %rbp\n"
        "    .cfi_def_cfa_register %rbp\n"
        "    sub $192, %rsp\n"
        "    mov %rdi, 0(%rsp)\n"
        "    mov %rsi, 8(%rsp)\n"
        "    mov %rdx, 16(%rsp)\n"
        "    mov %rcx, 24(%rsp)\n"
        "    mov %r8, 32(%rsp)\n"
        "    mov %r9, 40(%rsp)\n"
        "    mov 16(%rbp), %rax\n"
        "    mov %rax, 48(%rsp)\n"
        "    mov 24(%rbp), %rax\n"
        "    mov %rax, 56(%rsp)\n"
        "    movdqu %xmm0, 64(%rsp)\n"
        "    movdqu %xmm1, 80(%rsp)\n"
        "    movdqu %xmm2, 96(%rsp)\n"
        "    movdqu %xmm3, 112(%rsp)\n"
        "    movdqu %xmm4, 128(%rsp)\n"
        "    movdqu %xmm5, 144(%rsp)\n"
        "    movdqu %xmm6, 160(%rsp)\n"
        "    movdqu %xmm7, 176(%rsp)\n"
        "    mov %r11, %rdi\n"
        "    mov %rsp, %rsi\n"
        "    call ps5lift_native_call\n"
        "    mov 0(%rsp), %rax\n"
        "    mov 8(%rsp), %rdx\n"
        "    movdqu 64(%rsp), %xmm0\n"
        "    movdqu 80(%rsp), %xmm1\n"
        "    leave\n"
        "    .cfi_def_cfa %rsp, 8\n"
        "    ret\n"
        "    .cfi_endproc\n"
        ".size ps5lift_native_call_trampoline, . - ps5lift_native_call_trampoline\n");
#else
#error "native calls into guest code need a trampoline for this host"
#endif

namespace {

void Redirect(const int signal, siginfo_t* const info, void* const context) {
    auto& machine = static_cast<ucontext_t*>(context)->uc_mcontext;
#if defined(__aarch64__)
    auto& pc = machine.pc;
    auto& scratch = machine.regs[16];
#elif defined(__x86_64__)
    auto& pc = machine.gregs[REG_RIP];
    auto& scratch = machine.gregs[REG_R11];
#endif
    const auto address = static_cast<std::uint64_t>(pc);
    if (reinterpret_cast<std::uint64_t>(info->si_addr) == address && InsideGuestImage(address)) {
        if (Ps5Lift::LookupFunction(address) == nullptr) ps5lift_fail("native code called guest address 0x%" PRIx64 ", which is not a lifted function", address);
        scratch = address;
        pc = reinterpret_cast<std::uint64_t>(&ps5lift_native_call_trampoline);
        return;
    }
    const auto& previous = signal == SIGBUS ? PreviousBus : PreviousSegv;
    if ((previous.sa_flags & SA_SIGINFO) != 0) return previous.sa_sigaction(signal, info, context);
    if (previous.sa_handler == SIG_DFL || previous.sa_handler == SIG_IGN) {
        struct sigaction fallback {};
        fallback.sa_handler = SIG_DFL;
        sigemptyset(&fallback.sa_mask);
        sigaction(signal, &fallback, nullptr);
        return;
    }
    previous.sa_handler(signal);
}

}

namespace Ps5Lift {

void InstallNativeCalls() {
    struct sigaction action {};
    action.sa_sigaction = Redirect;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGSEGV, &action, &PreviousSegv) != 0 || sigaction(SIGBUS, &action, &PreviousBus) != 0) ps5lift_fail("cannot install the handler for native calls into guest code: %s", std::strerror(errno));
}

}
