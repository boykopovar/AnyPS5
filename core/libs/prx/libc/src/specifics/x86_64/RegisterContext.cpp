#if !defined(__linux__) || !defined(__x86_64__)
#error "This file is x86_64 Linux only"
#endif

#include <cstdint>

extern "C" void LibcCaptureRegisters(std::uintptr_t*) __attribute__((visibility("hidden")));
extern "C" [[noreturn]] void LibcRestoreRegisters(const std::uintptr_t*) __attribute__((visibility("hidden")));

asm(
".text\n"
".hidden LibcCaptureRegisters\n"
".type LibcCaptureRegisters,@function\n"
"LibcCaptureRegisters:\n"
"movq %rax,0(%rdi)\nmovq %rdx,8(%rdi)\nmovq %rcx,16(%rdi)\nmovq %rbx,24(%rdi)\n"
"movq %rsi,32(%rdi)\nmovq %rdi,40(%rdi)\nmovq %rbp,48(%rdi)\n"
"leaq 8(%rsp),%rax\nmovq %rax,56(%rdi)\n"
"movq %r8,64(%rdi)\nmovq %r9,72(%rdi)\nmovq %r10,80(%rdi)\nmovq %r11,88(%rdi)\n"
"movq %r12,96(%rdi)\nmovq %r13,104(%rdi)\nmovq %r14,112(%rdi)\nmovq %r15,120(%rdi)\n"
"movq (%rsp),%rax\nmovq %rax,128(%rdi)\nret\n"
".size LibcCaptureRegisters,.-LibcCaptureRegisters\n"
".hidden LibcRestoreRegisters\n.type LibcRestoreRegisters,@function\n"
"LibcRestoreRegisters:\n"
"movq %rdi,%r10\nmovq 128(%r10),%r11\n"
"movq 0(%r10),%rax\nmovq 8(%r10),%rdx\nmovq 16(%r10),%rcx\nmovq 24(%r10),%rbx\n"
"movq 32(%r10),%rsi\nmovq 40(%r10),%rdi\nmovq 48(%r10),%rbp\nmovq 56(%r10),%rsp\n"
"movq 64(%r10),%r8\nmovq 72(%r10),%r9\nmovq 96(%r10),%r12\n"
"movq 104(%r10),%r13\nmovq 112(%r10),%r14\nmovq 120(%r10),%r15\njmp *%r11\n"
".size LibcRestoreRegisters,.-LibcRestoreRegisters\n"
);
