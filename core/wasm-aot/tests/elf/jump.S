    .intel_syntax noprefix
    .text
    .globl guest_setjmp
    .type guest_setjmp, @function
guest_setjmp:
    .cfi_startproc
    mov [rdi], rbx
    mov [rdi + 8], rbp
    mov [rdi + 16], r12
    mov [rdi + 24], r13
    mov [rdi + 32], r14
    mov [rdi + 40], r15
    lea rdx, [rsp + 8]
    mov [rdi + 48], rdx
    mov rdx, [rsp]
    mov [rdi + 56], rdx
    xor eax, eax
    ret
    .cfi_endproc

    .globl guest_longjmp
    .type guest_longjmp, @function
guest_longjmp:
    .cfi_startproc
    mov rbx, [rdi]
    mov rbp, [rdi + 8]
    mov r12, [rdi + 16]
    mov r13, [rdi + 24]
    mov r14, [rdi + 32]
    mov r15, [rdi + 40]
    mov rsp, [rdi + 48]
    mov rax, rsi
    jmp qword ptr [rdi + 56]
    .cfi_endproc
