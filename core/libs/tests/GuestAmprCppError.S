.cfi_sections .eh_frame
.text
.globl GuestCatch
GuestCatch:
    .cfi_startproc
#ifdef _WIN32
    .cfi_personality 0x00, __gxx_personality_v0_nid_postfix
    .cfi_lsda 0x00, .Lcatch_lsda
#else
    .cfi_personality 0x9b, .Lcatch_personality
    .cfi_lsda 0x1b, .Lcatch_lsda
#endif
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $16, %rsp
.Lcatch_call:
    call _ZSt16_Throw_Cpp_errori_nid_postfix
.Lcatch_call_end:
    ud2
.Lcatch_landing:
    cmpq $2, %rdx
    jne .Lcatch_bad
    movq %rax, %rdi
    call __cxa_begin_catch_nid_postfix
    movq %rax, %rdi
    call FixtureInspect
    call __cxa_end_catch_nid_postfix
    leave
    .cfi_def_cfa %rsp, 8
    ret
.Lcatch_bad:
    movl $1, %edi
    call FixtureFail
    ud2
    .cfi_endproc
#ifdef _WIN32
.section .rdata,"dr"
#else
.section .data.rel.ro,"aw",@progbits
.p2align 3
.Lcatch_personality:
    .quad __gxx_personality_v0_nid_postfix
.section .rodata
#endif
.Lcatch_lsda:
#ifdef _WIN32
    .byte 0xff, 0x80
#else
    .byte 0xff, 0x9b
#endif
    .uleb128 .Lcatch_types_end-.Lcatch_types_offset
.Lcatch_types_offset:
    .byte 0x01
    .uleb128 .Lcatch_calls_end-.Lcatch_calls
.Lcatch_calls:
    .uleb128 .Lcatch_call-GuestCatch
    .uleb128 .Lcatch_call_end-.Lcatch_call
    .uleb128 .Lcatch_landing-GuestCatch
    .uleb128 1
.Lcatch_calls_end:
    .byte 1, 1, 2, 0
#ifdef _WIN32
    .quad FixtureCatchType
    .quad FixtureOtherType
#else
    .long FixtureCatchType-.
    .long FixtureOtherType-.
#endif
.Lcatch_types_end:
#ifndef _WIN32
.section .note.GNU-stack,"",@progbits
#endif
