.cfi_sections .eh_frame
.text
.globl SceV2Catch
SceV2Catch:
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
    movq %rdi, %rax
    movq %rsi, %rdi
.Lcatch_call:
    call *%rax
.Lcatch_call_end:
    ud2
.Lcatch_landing:
    movl %edx, SceV2Selector(%rip)
    movq %rax, %rdi
    call __cxa_begin_catch_nid_postfix
    movq %rax, %rdi
    call SceV2Inspect
    call __cxa_end_catch_nid_postfix
    leave
    .cfi_def_cfa %rsp, 8
    ret
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
    .uleb128 .Lcatch_call-SceV2Catch
    .uleb128 .Lcatch_call_end-.Lcatch_call
    .uleb128 .Lcatch_landing-SceV2Catch
    .uleb128 1
.Lcatch_calls_end:
    .byte 1, 1, 2, 0
#ifdef _WIN32
    .quad SceV2CatchType
    .quad SceV2OtherType
#else
    .long SceV2CatchType-.
    .long SceV2OtherType-.
#endif
.Lcatch_types_end:
#ifndef _WIN32
.section .note.GNU-stack,"",@progbits
#endif
