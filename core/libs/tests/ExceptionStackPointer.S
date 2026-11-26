.cfi_sections .eh_frame
.text
.p2align 4

.globl ExceptionStackPointerFrame
.type ExceptionStackPointerFrame, @function
ExceptionStackPointerFrame:
.Lframe:
    .cfi_startproc
    .cfi_personality 0x9b, DW.ref.__gxx_personality_v0
    .cfi_lsda 0x1b, .LSDA
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset 6, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register 6
    pushq %rbx
    .cfi_offset 3, -24
    subq $8, %rsp
    movq %rdi, %rbx
    movl $1, %edi
    movl $2, %esi
    movl $3, %edx
    movl $4, %ecx
    movl $5, %r8d
    movl $6, %r9d
    pushq $10
    pushq $9
    pushq $8
    pushq $7
.Lcall:
    .cfi_escape 0x2e, 0x20
    call *%rbx
.Lreturn:
    addq $0x20, %rsp
    xorl %eax, %eax
.Lepilogue:
    addq $8, %rsp
    popq %rbx
    popq %rbp
    ret
.Llanding:
    movq %rax, %rdi
    call __cxa_begin_catch@PLT
    call __cxa_end_catch@PLT
    leaq -16(%rbp), %rcx
    cmpq %rcx, %rsp
    jne .Lmisplaced
    movl $1, %eax
    jmp .Lepilogue
.Lmisplaced:
    movl $2, %eax
    jmp .Lepilogue
    .cfi_endproc
.size ExceptionStackPointerFrame, .-ExceptionStackPointerFrame

.section .gcc_except_table,"a",@progbits
.p2align 2
.LSDA:
    .byte 0xff
    .byte 0x9b
    .uleb128 .Ltype_end - .Ltype_base
.Ltype_base:
    .byte 0x1
    .uleb128 .Lcalls_end - .Lcalls
.Lcalls:
    .uleb128 .Lcall - .Lframe
    .uleb128 .Lreturn - .Lcall
    .uleb128 .Llanding - .Lframe
    .uleb128 0x1
.Lcalls_end:
    .byte 0x1
    .byte 0
    .p2align 2
    .long DW.ref._ZTIi - .
.Ltype_end:

.hidden DW.ref._ZTIi
.weak DW.ref._ZTIi
.section .data.rel.local.DW.ref._ZTIi,"awG",@progbits,DW.ref._ZTIi,comdat
.p2align 3
.type DW.ref._ZTIi, @object
.size DW.ref._ZTIi, 8
DW.ref._ZTIi:
    .quad _ZTIi

.hidden DW.ref.__gxx_personality_v0
.weak DW.ref.__gxx_personality_v0
.section .data.rel.local.DW.ref.__gxx_personality_v0,"awG",@progbits,DW.ref.__gxx_personality_v0,comdat
.p2align 3
.type DW.ref.__gxx_personality_v0, @object
.size DW.ref.__gxx_personality_v0, 8
DW.ref.__gxx_personality_v0:
    .quad __gxx_personality_v0

.section .note.GNU-stack,"",@progbits
