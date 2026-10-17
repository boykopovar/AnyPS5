    .intel_syntax noprefix
    .text
    .globl _oracle_page
    .p2align 12
_oracle_page:

exit:
    mov [rip + gprs + 0x00], rax
    mov [rip + gprs + 0x08], rcx
    mov [rip + gprs + 0x10], rdx
    mov [rip + gprs + 0x18], rbx
    mov [rip + gprs + 0x20], rsp
    mov [rip + gprs + 0x28], rbp
    mov [rip + gprs + 0x30], rsi
    mov [rip + gprs + 0x38], rdi
    mov [rip + gprs + 0x40], r8
    mov [rip + gprs + 0x48], r9
    mov [rip + gprs + 0x50], r10
    mov [rip + gprs + 0x58], r11
    mov [rip + gprs + 0x60], r12
    mov [rip + gprs + 0x68], r13
    mov [rip + gprs + 0x70], r14
    mov [rip + gprs + 0x78], r15
    lea rsp, [rip + private_stack_top]
    pushfq
    pop rax
    mov [rip + rflags_out], rax
    vmovdqu [rip + ymms + 0x000], ymm0
    vmovdqu [rip + ymms + 0x020], ymm1
    vmovdqu [rip + ymms + 0x040], ymm2
    vmovdqu [rip + ymms + 0x060], ymm3
    vmovdqu [rip + ymms + 0x080], ymm4
    vmovdqu [rip + ymms + 0x0a0], ymm5
    vmovdqu [rip + ymms + 0x0c0], ymm6
    vmovdqu [rip + ymms + 0x0e0], ymm7
    vmovdqu [rip + ymms + 0x100], ymm8
    vmovdqu [rip + ymms + 0x120], ymm9
    vmovdqu [rip + ymms + 0x140], ymm10
    vmovdqu [rip + ymms + 0x160], ymm11
    vmovdqu [rip + ymms + 0x180], ymm12
    vmovdqu [rip + ymms + 0x1a0], ymm13
    vmovdqu [rip + ymms + 0x1c0], ymm14
    vmovdqu [rip + ymms + 0x1e0], ymm15
    vzeroupper
    fninit
    mov rsp, [rip + host_rsp]
    mov rbx, [rip + host_rbx]
    mov rbp, [rip + host_rbp]
    mov r12, [rip + host_r12]
    mov r13, [rip + host_r13]
    mov r14, [rip + host_r14]
    mov r15, [rip + host_r15]
    ret

    .org 0x200
entry:
    mov [rip + host_rsp], rsp
    mov [rip + host_rbx], rbx
    mov [rip + host_rbp], rbp
    mov [rip + host_r12], r12
    mov [rip + host_r13], r13
    mov [rip + host_r14], r14
    mov [rip + host_r15], r15
    vmovdqu ymm0, [rip + ymms + 0x000]
    vmovdqu ymm1, [rip + ymms + 0x020]
    vmovdqu ymm2, [rip + ymms + 0x040]
    vmovdqu ymm3, [rip + ymms + 0x060]
    vmovdqu ymm4, [rip + ymms + 0x080]
    vmovdqu ymm5, [rip + ymms + 0x0a0]
    vmovdqu ymm6, [rip + ymms + 0x0c0]
    vmovdqu ymm7, [rip + ymms + 0x0e0]
    vmovdqu ymm8, [rip + ymms + 0x100]
    vmovdqu ymm9, [rip + ymms + 0x120]
    vmovdqu ymm10, [rip + ymms + 0x140]
    vmovdqu ymm11, [rip + ymms + 0x160]
    vmovdqu ymm12, [rip + ymms + 0x180]
    vmovdqu ymm13, [rip + ymms + 0x1a0]
    vmovdqu ymm14, [rip + ymms + 0x1c0]
    vmovdqu ymm15, [rip + ymms + 0x1e0]
    push qword ptr [rip + rflags_in]
    popfq
    mov rsp, [rip + gprs + 0x20]
    mov rax, [rip + gprs + 0x00]
    mov rcx, [rip + gprs + 0x08]
    mov rdx, [rip + gprs + 0x10]
    mov rbx, [rip + gprs + 0x18]
    mov rbp, [rip + gprs + 0x28]
    mov rsi, [rip + gprs + 0x30]
    mov rdi, [rip + gprs + 0x38]
    mov r8, [rip + gprs + 0x40]
    mov r9, [rip + gprs + 0x48]
    mov r10, [rip + gprs + 0x50]
    mov r11, [rip + gprs + 0x58]
    mov r12, [rip + gprs + 0x60]
    mov r13, [rip + gprs + 0x68]
    mov r14, [rip + gprs + 0x70]
    mov r15, [rip + gprs + 0x78]
    jmp qword ptr [rip + code_address]

    .org 0x400
host_rsp: .quad 0
host_rbx: .quad 0
host_rbp: .quad 0
host_r12: .quad 0
host_r13: .quad 0
host_r14: .quad 0
host_r15: .quad 0
code_address: .quad 0
rflags_in: .quad 0
rflags_out: .quad 0

    .org 0x500
gprs: .space 0x80

    .org 0x600
ymms: .space 0x200

    .org 0x1000
private_stack_top:
