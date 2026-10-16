    .intel_syntax noprefix
    .section .wxcode, "awx"
    .globl writable_init
    .type writable_init, @function
writable_init:
    lea rax, [rip + init_marker]
    mov qword ptr [rax], 4242
    ret

    .data
init_marker:
    .quad 0

    .text
    .globl read_init_marker
    .type read_init_marker, @function
read_init_marker:
    mov rax, qword ptr [rip + init_marker]
    ret
