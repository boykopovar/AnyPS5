#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#ifdef __EMSCRIPTEN__
#include "wasm-runtime/Guest.hpp"
#include <cstring>
#endif
#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" int* APS5_VABI __error_nid_postfix();

struct GuestSignalSet {
    std::uint32_t bits[4];
};
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
#ifdef __EMSCRIPTEN__
extern "C" int APS5_VABI _sigprocmask_nid_postfix(int how, const GuestSignalSet* set, GuestSignalSet* old);

namespace {

constexpr unsigned SignalJumpRegisters[] = {WasmRuntime::Rbx, WasmRuntime::Rsp, WasmRuntime::Rbp, WasmRuntime::R12, WasmRuntime::R13, WasmRuntime::R14, WasmRuntime::R15};
constexpr std::size_t SignalMaskOffset = 72;
constexpr std::size_t SaveMaskOffset = 88;
constexpr int GuestBlockSignals = 1;
constexpr int GuestSetSignalMask = 3;

}

extern "C" {

int APS5_VABI sigsetjmp_nid_postfix(std::uint64_t* buffer, int saveMask) {
    auto* bytes = reinterpret_cast<unsigned char*>(buffer);
    std::memcpy(bytes + SaveMaskOffset, &saveMask, sizeof(saveMask));
    if (saveMask != 0) _sigprocmask_nid_postfix(GuestBlockSignals, nullptr, reinterpret_cast<GuestSignalSet*>(bytes + SignalMaskOffset));
    const auto& cpu = WasmRuntime::CurrentCpu();
    std::memcpy(&buffer[0], reinterpret_cast<const void*>(static_cast<std::uintptr_t>(cpu.gpr[WasmRuntime::Rsp])), sizeof(buffer[0]));
    for (unsigned slot = 0; slot < 7; ++slot) buffer[1 + slot] = cpu.gpr[SignalJumpRegisters[slot]];
    std::memcpy(&buffer[8], &cpu.x87Fcw, sizeof(cpu.x87Fcw));
    return 0;
}

int APS5_VABI siglongjmp_nid_postfix(std::uint64_t* buffer, int value) {
    auto* bytes = reinterpret_cast<unsigned char*>(buffer);
    int saveMask;
    std::memcpy(&saveMask, bytes + SaveMaskOffset, sizeof(saveMask));
    if (saveMask != 0) _sigprocmask_nid_postfix(GuestSetSignalMask, reinterpret_cast<const GuestSignalSet*>(bytes + SignalMaskOffset), nullptr);
    auto& cpu = WasmRuntime::CurrentCpu();
    for (unsigned slot = 0; slot < 7; ++slot) cpu.gpr[SignalJumpRegisters[slot]] = buffer[1 + slot];
    std::memcpy(&cpu.x87Fcw, &buffer[8], sizeof(cpu.x87Fcw));
    const int result = value == 0 ? 1 : value;
    cpu.gpr[WasmRuntime::Rax] = static_cast<std::uint32_t>(result);
    WasmRuntime::TransferGuest(buffer[0], buffer[2] + 8);
    return result;
}

}
#else
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
#endif
