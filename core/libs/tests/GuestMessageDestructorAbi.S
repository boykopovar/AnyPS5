.cfi_sections .eh_frame
.text
.macro PROLOGUE
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $16, %rsp
.endm
.macro WHAT
    movq %rax, %rdi
    movq (%rax), %r11
    xorl %ecx, %ecx
    call *16(%r11)
    movq %rax, %rdi
    call FixtureCheckWhat
.endm
.globl GuestThrow
GuestThrow:
    .cfi_startproc
    PROLOGUE
    movl $16, %edi
    call __cxa_allocate_exception_nid_postfix
    movq %rax, FixtureLastObject(%rip)
    leaq FixtureVtable+16(%rip), %rdx
    movq %rdx, (%rax)
    leaq FixtureMessage(%rip), %rdx
    movq %rdx, 8(%rax)
    movq %rax, %rdi
    leaq FixtureErrorType(%rip), %rsi
    leaq FixtureDestroy(%rip), %rdx
    call __cxa_throw_nid_postfix
    ud2
    .cfi_endproc

.globl GuestInner
GuestInner:
    .cfi_startproc
    .cfi_personality 0x00, __gxx_personality_v0_nid_postfix
    .cfi_lsda 0x00, .Linner_lsda
    PROLOGUE
.Linner_call:
    call GuestThrow
.Linner_call_end:
    ud2
.Linner_catch:
    cmpq $1, %rdx
    jne .Linner_bad
    movq %rax, %rdi
    call __cxa_begin_catch_nid_postfix
    incl FixtureCaught(%rip)
    WHAT
.Linner_rethrow:
    call __cxa_rethrow_nid_postfix
.Linner_rethrow_end:
    ud2
.Linner_cleanup:
    movq %rax, -8(%rbp)
    call __cxa_end_catch_nid_postfix
    incl FixtureGuardDestroyed(%rip)
    movq -8(%rbp), %rdi
    call _Unwind_Resume_nid_postfix
    ud2
.Linner_bad:
    movl $31, %edi
    call FixtureFail
    ud2
    .cfi_endproc

.globl GuestOuter
GuestOuter:
    .cfi_startproc
    .cfi_personality 0x00, __gxx_personality_v0_nid_postfix
    .cfi_lsda 0x00, .Louter_lsda
    PROLOGUE
.Louter_call:
    call GuestInner
.Louter_call_end:
    ud2
.Louter_catch:
    cmpq $2, %rdx
    jne .Louter_bad
    movq %rax, %rdi
    call __cxa_begin_catch_nid_postfix
    cmpq FixtureLastObject(%rip), %rax
    jne .Louter_bad
    incl FixtureCaught(%rip)
    WHAT
    call __cxa_end_catch_nid_postfix
    xorl %eax, %eax
    leave
    .cfi_def_cfa %rsp, 8
    ret
.Louter_bad:
    movl $32, %edi
    call FixtureFail
    ud2
    .cfi_endproc

.globl GuestStdCapture
GuestStdCapture:
    .cfi_startproc
    .cfi_personality 0x00, __gxx_personality_v0_nid_postfix
    .cfi_lsda 0x00, .Lstd_lsda
    PROLOGUE
    movq FixtureArgument(%rip), %rdi
.Lstd_call:
    call *FixtureThrow(%rip)
.Lstd_call_end:
    ud2
.Lstd_catch:
    cmpq $1, %rdx
    jne .Lstd_bad
    movq %rax, %rdi
    call __cxa_begin_catch_nid_postfix
    movq %rax, %rdi
    call FixtureInspect
    call __cxa_end_catch_nid_postfix
    leave
    .cfi_def_cfa %rsp, 8
    ret
.Lstd_bad:
    movl $33, %edi
    call FixtureFail
    ud2
    .cfi_endproc

.globl ProbeWhat
ProbeWhat:
.globl ProbeDestroy
ProbeDestroy:
    .cfi_startproc
    PROLOGUE
    subq $32, %rsp
    movq %rdi, %r11
    movq %rsi, %rdi
    movq %rdx, %rcx
    call *%r11
    leave
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
.section .rdata,"dr"
.Linner_lsda:
    .byte 0xff, 0x00
    .uleb128 .Linner_types_end-.Linner_types_offset
.Linner_types_offset:
    .byte 0x01
    .uleb128 .Linner_calls_end-.Linner_calls
.Linner_calls:
    .uleb128 .Linner_call-GuestInner
    .uleb128 .Linner_call_end-.Linner_call
    .uleb128 .Linner_catch-GuestInner
    .uleb128 1
    .uleb128 .Linner_rethrow-GuestInner
    .uleb128 .Linner_rethrow_end-.Linner_rethrow
    .uleb128 .Linner_cleanup-GuestInner
    .uleb128 0
.Linner_calls_end:
    .byte 1, 0
    .quad FixtureErrorType
.Linner_types_end:
.Louter_lsda:
    .byte 0xff, 0x00
    .uleb128 .Louter_types_end-.Louter_types_offset
.Louter_types_offset:
    .byte 0x01
    .uleb128 .Louter_calls_end-.Louter_calls
.Louter_calls:
    .uleb128 .Louter_call-GuestOuter
    .uleb128 .Louter_call_end-.Louter_call
    .uleb128 .Louter_catch-GuestOuter
    .uleb128 1
.Louter_calls_end:
    .byte 1, 1, 2, 0
    .quad FixtureBaseType
    .quad FixtureOtherType
.Louter_types_end:
.Lstd_lsda:
    .byte 0xff, 0x80
    .uleb128 .Lstd_types_end-.Lstd_types_offset
.Lstd_types_offset:
    .byte 0x01
    .uleb128 .Lstd_calls_end-.Lstd_calls
.Lstd_calls:
    .uleb128 .Lstd_call-GuestStdCapture
    .uleb128 .Lstd_call_end-.Lstd_call
    .uleb128 .Lstd_catch-GuestStdCapture
    .uleb128 1
.Lstd_calls_end:
    .byte 1, 0
    .quad FixtureStdOutType
.Lstd_types_end:
