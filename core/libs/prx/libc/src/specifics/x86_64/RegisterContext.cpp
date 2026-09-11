#include <cstdint>
#include <cstdlib>

extern "C" void LibcCaptureRegisters(std::uintptr_t*);
extern "C" [[noreturn]] void LibcRestoreRegisters(const std::uintptr_t*);

#if defined(__linux__) && defined(__x86_64__)
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
#elif defined(_WIN32) && defined(__x86_64__)
asm(
".text\n"
".globl LibcCaptureRegisters\n"
"LibcCaptureRegisters:\n"
"movq %rax,0(%rcx)\nmovq %rdx,8(%rcx)\nmovq %rcx,16(%rcx)\nmovq %rbx,24(%rcx)\n"
"movq %rsi,32(%rcx)\nmovq %rdi,40(%rcx)\nmovq %rbp,48(%rcx)\n"
"leaq 8(%rsp),%rax\nmovq %rax,56(%rcx)\n"
"movq %r8,64(%rcx)\nmovq %r9,72(%rcx)\nmovq %r10,80(%rcx)\nmovq %r11,88(%rcx)\n"
"movq %r12,96(%rcx)\nmovq %r13,104(%rcx)\nmovq %r14,112(%rcx)\nmovq %r15,120(%rcx)\n"
"movdqu %xmm0,136(%rcx)\n"
"movdqu %xmm1,152(%rcx)\n"
"movdqu %xmm2,168(%rcx)\n"
"movdqu %xmm3,184(%rcx)\n"
"movdqu %xmm4,200(%rcx)\n"
"movdqu %xmm5,216(%rcx)\n"
"movdqu %xmm6,232(%rcx)\n"
"movdqu %xmm7,248(%rcx)\n"
"movdqu %xmm8,264(%rcx)\n"
"movdqu %xmm9,280(%rcx)\n"
"movdqu %xmm10,296(%rcx)\n"
"movdqu %xmm11,312(%rcx)\n"
"movdqu %xmm12,328(%rcx)\n"
"movdqu %xmm13,344(%rcx)\n"
"movdqu %xmm14,360(%rcx)\n"
"movdqu %xmm15,376(%rcx)\n"
"movq (%rsp),%rax\nmovq %rax,128(%rcx)\nret\n"
".globl LibcRestoreRegisters\n"
"LibcRestoreRegisters:\n"
"movq %rcx,%r10\nmovq 128(%r10),%r11\n"
"movdqu 136(%r10),%xmm0\n"
"movdqu 152(%r10),%xmm1\n"
"movdqu 168(%r10),%xmm2\n"
"movdqu 184(%r10),%xmm3\n"
"movdqu 200(%r10),%xmm4\n"
"movdqu 216(%r10),%xmm5\n"
"movdqu 232(%r10),%xmm6\n"
"movdqu 248(%r10),%xmm7\n"
"movdqu 264(%r10),%xmm8\n"
"movdqu 280(%r10),%xmm9\n"
"movdqu 296(%r10),%xmm10\n"
"movdqu 312(%r10),%xmm11\n"
"movdqu 328(%r10),%xmm12\n"
"movdqu 344(%r10),%xmm13\n"
"movdqu 360(%r10),%xmm14\n"
"movdqu 376(%r10),%xmm15\n"
"movq 0(%r10),%rax\nmovq 8(%r10),%rdx\nmovq 16(%r10),%rcx\nmovq 24(%r10),%rbx\n"
"movq 32(%r10),%rsi\nmovq 40(%r10),%rdi\nmovq 48(%r10),%rbp\nmovq 56(%r10),%rsp\n"
"movq 64(%r10),%r8\nmovq 72(%r10),%r9\nmovq 96(%r10),%r12\n"
"movq 104(%r10),%r13\nmovq 112(%r10),%r14\nmovq 120(%r10),%r15\njmp *%r11\n"
);
#else
void LibcCaptureRegisters(std::uintptr_t*) { std::abort(); }
[[noreturn]] void LibcRestoreRegisters(const std::uintptr_t*) { std::abort(); }
#endif
