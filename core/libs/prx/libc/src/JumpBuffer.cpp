#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#ifdef __EMSCRIPTEN__
#include "wasm-runtime/Guest.hpp"
#include <cstdint>
#include <cstring>
#endif

// setjmp/longjmp must capture the guest's own frame, so they are written directly in assembly with
// the guest (System V) calling convention. The saved state fits the guest's 96-byte jmp_buf:
// return address, rbx, rsp, rbp, r12-r15, MXCSR and the x87 control word.

#ifdef __EMSCRIPTEN__
namespace {

constexpr unsigned JumpBufferRegisters[] = {WasmRuntime::Rbx, WasmRuntime::Rsp, WasmRuntime::Rbp, WasmRuntime::R12, WasmRuntime::R13, WasmRuntime::R14, WasmRuntime::R15};

}

extern "C" {

int APS5_VABI setjmp_nid_postfix(std::uint64_t* buffer) {
    const auto& cpu = WasmRuntime::CurrentCpu();
    std::memcpy(&buffer[0], reinterpret_cast<const void*>(static_cast<std::uintptr_t>(cpu.gpr[WasmRuntime::Rsp])), sizeof(buffer[0]));
    for (unsigned slot = 0; slot < 7; ++slot) buffer[1 + slot] = cpu.gpr[JumpBufferRegisters[slot]];
    buffer[2] += 8;
    std::memcpy(&buffer[8], &cpu.mxcsr, sizeof(cpu.mxcsr));
    std::memcpy(reinterpret_cast<unsigned char*>(&buffer[8]) + 4, &cpu.x87Fcw, sizeof(cpu.x87Fcw));
    return 0;
}

int APS5_VABI longjmp_nid_postfix(const std::uint64_t* buffer, int value) {
    auto& cpu = WasmRuntime::CurrentCpu();
    for (unsigned slot = 0; slot < 7; ++slot) cpu.gpr[JumpBufferRegisters[slot]] = buffer[1 + slot];
    std::memcpy(&cpu.mxcsr, &buffer[8], sizeof(cpu.mxcsr));
    std::memcpy(&cpu.x87Fcw, reinterpret_cast<const unsigned char*>(&buffer[8]) + 4, sizeof(cpu.x87Fcw));
    const int result = value == 0 ? 1 : value;
    cpu.gpr[WasmRuntime::Rax] = static_cast<std::uint32_t>(result);
    WasmRuntime::TransferGuest(buffer[0], buffer[2]);
    return result;
}

}
#else
asm(".text\n"
    APS5_ASM_FUNCTION("setjmp_nid_postfix")
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
#ifdef _WIN32
    "    mov %rdi, %rbx\n"
    "    mov %esi, %r12d\n"
    "    mov 16(%rdi), %rdi\n"
    "    and $-16, %rsp\n"
    "    call GuestStackSwitch_nid_no_patch\n"
    "    mov %rbx, %rdi\n"
    "    mov %r12d, %esi\n"
#endif
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
#endif
