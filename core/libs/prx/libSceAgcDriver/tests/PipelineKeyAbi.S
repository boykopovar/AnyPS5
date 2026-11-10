.cfi_sections .eh_frame
.text
.p2align 4
.globl PipelineKeyAbiProbe
#ifdef _WIN32
.seh_proc PipelineKeyAbiProbe
#else
.type PipelineKeyAbiProbe, @function
#endif
PipelineKeyAbiProbe:
.cfi_startproc
    subq $776, %rsp
.cfi_def_cfa_offset 784
#ifdef _WIN32
.seh_stackalloc 776
#endif
    movq %rbx, 32(%rsp)
.cfi_offset %rbx, -752
#ifdef _WIN32
.seh_savereg %rbx, 32
#endif
    movq %rbp, 40(%rsp)
.cfi_offset %rbp, -744
#ifdef _WIN32
.seh_savereg %rbp, 40
#endif
    movq %r12, 48(%rsp)
.cfi_offset %r12, -736
#ifdef _WIN32
.seh_savereg %r12, 48
#endif
    movq %r13, 56(%rsp)
.cfi_offset %r13, -728
#ifdef _WIN32
.seh_savereg %r13, 56
#endif
    movq %r14, 64(%rsp)
.cfi_offset %r14, -720
#ifdef _WIN32
.seh_savereg %r14, 64
#endif
    movq %r15, 72(%rsp)
.cfi_offset %r15, -712
#ifdef _WIN32
.seh_savereg %r15, 72
#endif
    movq %rdi, 80(%rsp)
.cfi_offset %rdi, -704
#ifdef _WIN32
.seh_savereg %rdi, 80
#endif
    movq %rsi, 88(%rsp)
.cfi_offset %rsi, -696
#ifdef _WIN32
.seh_savereg %rsi, 88
#endif
#ifdef _WIN32
    movdqu %xmm6, 96(%rsp)
.cfi_offset %xmm6, -688
.seh_savexmm %xmm6, 96
    movdqu %xmm7, 112(%rsp)
.cfi_offset %xmm7, -672
.seh_savexmm %xmm7, 112
    movdqu %xmm8, 128(%rsp)
.cfi_offset %xmm8, -656
.seh_savexmm %xmm8, 128
    movdqu %xmm9, 144(%rsp)
.cfi_offset %xmm9, -640
.seh_savexmm %xmm9, 144
    movdqu %xmm10, 160(%rsp)
.cfi_offset %xmm10, -624
.seh_savexmm %xmm10, 160
    movdqu %xmm11, 176(%rsp)
.cfi_offset %xmm11, -608
.seh_savexmm %xmm11, 176
    movdqu %xmm12, 192(%rsp)
.cfi_offset %xmm12, -592
.seh_savexmm %xmm12, 192
    movdqu %xmm13, 208(%rsp)
.cfi_offset %xmm13, -576
.seh_savexmm %xmm13, 208
    movdqu %xmm14, 224(%rsp)
.cfi_offset %xmm14, -560
.seh_savexmm %xmm14, 224
    movdqu %xmm15, 240(%rsp)
.cfi_offset %xmm15, -544
.seh_savexmm %xmm15, 240
.seh_endprologue
    movq %rcx, 256(%rsp)
    movq %rdx, 264(%rsp)
    movq %r8, 272(%rsp)
    movq %r9, 280(%rsp)
    movq 816(%rsp), %rax
    movq %rax, 288(%rsp)
    movq 824(%rsp), %rax
    movq %rax, 760(%rsp)
#else
    movq %rdi, 256(%rsp)
    movq %rsi, 264(%rsp)
    movq %rdx, 272(%rsp)
    movq %rcx, 280(%rsp)
    movq %r8, 288(%rsp)
    movq %r9, 760(%rsp)
#endif
    leaq 304(%rsp), %rdi
    movl $56, %ecx
    xorl %eax, %eax
    rep stosq
    movq 760(%rsp), %rax
    movabsq $0x1234567887654321, %rbx
    xorq %rax, %rbx
    movq %rbx, 304(%rsp)
    movabsq $0x1335577988664422, %rbp
    xorq %rax, %rbp
    movq %rbp, 312(%rsp)
    movabsq $0x1436587a89674523, %r12
    xorq %rax, %r12
    movq %r12, 320(%rsp)
    movabsq $0x1537597b8a684624, %r13
    xorq %rax, %r13
    movq %r13, 328(%rsp)
    movabsq $0x16385a7c8b694725, %r14
    xorq %rax, %r14
    movq %r14, 336(%rsp)
    movabsq $0x17395b7d8c6a4826, %r15
    xorq %rax, %r15
    movq %r15, 344(%rsp)
#ifdef _WIN32
    movabsq $0x183a5c7e8d6b4927, %rdi
    xorq %rax, %rdi
    movq %rdi, 352(%rsp)
    movabsq $0x193b5d7f8e6c4a28, %rsi
    xorq %rax, %rsi
    movq %rsi, 360(%rsp)
#endif
#ifdef _WIN32
    movq %rax, %xmm0
    punpcklqdq %xmm0, %xmm0
    movdqu .Lseed0(%rip), %xmm6
    pxor %xmm0, %xmm6
    movdqu %xmm6, 368(%rsp)
    movdqu .Lseed1(%rip), %xmm7
    pxor %xmm0, %xmm7
    movdqu %xmm7, 384(%rsp)
    movdqu .Lseed2(%rip), %xmm8
    pxor %xmm0, %xmm8
    movdqu %xmm8, 400(%rsp)
    movdqu .Lseed3(%rip), %xmm9
    pxor %xmm0, %xmm9
    movdqu %xmm9, 416(%rsp)
    movdqu .Lseed4(%rip), %xmm10
    pxor %xmm0, %xmm10
    movdqu %xmm10, 432(%rsp)
    movdqu .Lseed5(%rip), %xmm11
    pxor %xmm0, %xmm11
    movdqu %xmm11, 448(%rsp)
    movdqu .Lseed6(%rip), %xmm12
    pxor %xmm0, %xmm12
    movdqu %xmm12, 464(%rsp)
    movdqu .Lseed7(%rip), %xmm13
    pxor %xmm0, %xmm13
    movdqu %xmm13, 480(%rsp)
    movdqu .Lseed8(%rip), %xmm14
    pxor %xmm0, %xmm14
    movdqu %xmm14, 496(%rsp)
    movdqu .Lseed9(%rip), %xmm15
    pxor %xmm0, %xmm15
    movdqu %xmm15, 512(%rsp)
    movq 264(%rsp), %rcx
    movq 272(%rsp), %rdx
    movq 280(%rsp), %r8
#else
    movq 264(%rsp), %rdi
    movq 272(%rsp), %rsi
    movq 280(%rsp), %rdx
#endif
    call *256(%rsp)
    movq %rbx, 528(%rsp)
    movq %rbp, 536(%rsp)
    movq %r12, 544(%rsp)
    movq %r13, 552(%rsp)
    movq %r14, 560(%rsp)
    movq %r15, 568(%rsp)
#ifdef _WIN32
    movq %rdi, 576(%rsp)
    movq %rsi, 584(%rsp)
#endif
#ifdef _WIN32
    movdqu %xmm6, 592(%rsp)
    movdqu %xmm7, 608(%rsp)
    movdqu %xmm8, 624(%rsp)
    movdqu %xmm9, 640(%rsp)
    movdqu %xmm10, 656(%rsp)
    movdqu %xmm11, 672(%rsp)
    movdqu %xmm12, 688(%rsp)
    movdqu %xmm13, 704(%rsp)
    movdqu %xmm14, 720(%rsp)
    movdqu %xmm15, 736(%rsp)
#endif
    movl %eax, 752(%rsp)
    leaq 304(%rsp), %rsi
    movq 288(%rsp), %rdi
    movl $56, %ecx
    rep movsq
    movl 752(%rsp), %eax
    movl %eax, (%rdi)
#ifdef _WIN32
    movdqu 96(%rsp), %xmm6
.cfi_restore %xmm6
    movdqu 112(%rsp), %xmm7
.cfi_restore %xmm7
    movdqu 128(%rsp), %xmm8
.cfi_restore %xmm8
    movdqu 144(%rsp), %xmm9
.cfi_restore %xmm9
    movdqu 160(%rsp), %xmm10
.cfi_restore %xmm10
    movdqu 176(%rsp), %xmm11
.cfi_restore %xmm11
    movdqu 192(%rsp), %xmm12
.cfi_restore %xmm12
    movdqu 208(%rsp), %xmm13
.cfi_restore %xmm13
    movdqu 224(%rsp), %xmm14
.cfi_restore %xmm14
    movdqu 240(%rsp), %xmm15
.cfi_restore %xmm15
#endif
    movq 32(%rsp), %rbx
.cfi_restore %rbx
    movq 40(%rsp), %rbp
.cfi_restore %rbp
    movq 48(%rsp), %r12
.cfi_restore %r12
    movq 56(%rsp), %r13
.cfi_restore %r13
    movq 64(%rsp), %r14
.cfi_restore %r14
    movq 72(%rsp), %r15
.cfi_restore %r15
    movq 80(%rsp), %rdi
.cfi_restore %rdi
    movq 88(%rsp), %rsi
.cfi_restore %rsi
    addq $776, %rsp
.cfi_def_cfa_offset 8
    ret
.cfi_endproc
#ifdef _WIN32
.seh_endproc
#else
.size PipelineKeyAbiProbe, .-PipelineKeyAbiProbe
#endif
.p2align 4
.globl PipelineKeyAbiConforming
#ifdef _WIN32
.seh_proc PipelineKeyAbiConforming
#else
.type PipelineKeyAbiConforming, @function
#endif
PipelineKeyAbiConforming:
.cfi_startproc
#ifdef _WIN32
.seh_endprologue
    cmpq $1, %rcx
    jne .LbadPipelineKeyAbiConforming
    cmpq $2, %rdx
    jne .LbadPipelineKeyAbiConforming
    movl $1, 16(%r8)
    movb $0x5a, 20(%r8)
#else
    cmpq $1, %rdi
    jne .LbadPipelineKeyAbiConforming
    cmpq $2, %rsi
    jne .LbadPipelineKeyAbiConforming
    movl $1, 16(%rdx)
    movb $0x5a, 20(%rdx)
#endif
    xorl %eax, %eax
    ret
.LbadPipelineKeyAbiConforming:
    movl $-13, %eax
    ret
.cfi_endproc
#ifdef _WIN32
.seh_endproc
#else
.size PipelineKeyAbiConforming, .-PipelineKeyAbiConforming
#endif
.p2align 4
.globl PipelineKeyAbiClobbering
#ifdef _WIN32
.seh_proc PipelineKeyAbiClobbering
#else
.type PipelineKeyAbiClobbering, @function
#endif
PipelineKeyAbiClobbering:
.cfi_startproc
#ifdef _WIN32
.seh_endprologue
    cmpq $1, %rcx
    jne .LbadPipelineKeyAbiClobbering
    cmpq $2, %rdx
    jne .LbadPipelineKeyAbiClobbering
    movl $1, 16(%r8)
    movb $0x5a, 20(%r8)
#else
    cmpq $1, %rdi
    jne .LbadPipelineKeyAbiClobbering
    cmpq $2, %rsi
    jne .LbadPipelineKeyAbiClobbering
    movl $1, 16(%rdx)
    movb $0x5a, 20(%rdx)
#endif
    movabsq $0x1234567800000000, %rbx
    movabsq $0x1335577900000000, %rbp
    movabsq $0x1436587a00000000, %r12
    movabsq $0x1537597b00000000, %r13
    movabsq $0x16385a7c00000000, %r14
    movabsq $0x17395b7d00000000, %r15
#ifdef _WIN32
    movabsq $0x183a5c7e00000000, %rdi
    movabsq $0x193b5d7f00000000, %rsi
    pcmpeqd %xmm6, %xmm6
    pcmpeqd %xmm7, %xmm7
    pcmpeqd %xmm8, %xmm8
    pcmpeqd %xmm9, %xmm9
    pcmpeqd %xmm10, %xmm10
    pcmpeqd %xmm11, %xmm11
    pcmpeqd %xmm12, %xmm12
    pcmpeqd %xmm13, %xmm13
    pcmpeqd %xmm14, %xmm14
    pcmpeqd %xmm15, %xmm15
#endif
    xorl %eax, %eax
    ret
.LbadPipelineKeyAbiClobbering:
    movl $-13, %eax
    ret
.cfi_endproc
#ifdef _WIN32
.seh_endproc
#else
.size PipelineKeyAbiClobbering, .-PipelineKeyAbiClobbering
#endif
.p2align 4
.globl PipelineKeyAbiNested
#ifdef _WIN32
.seh_proc PipelineKeyAbiNested
#else
.type PipelineKeyAbiNested, @function
#endif
PipelineKeyAbiNested:
.cfi_startproc
#ifdef _WIN32
    subq $56, %rsp
.cfi_def_cfa_offset 64
.seh_stackalloc 56
.seh_endprologue
    movq %rdx, %r10
    movq %r8, %r9
    movq %rcx, %rdx
    movq $2, %r8
    movq 0(%r10), %rcx
    movq 8(%r10), %rax
    movq %rax, 32(%rsp)
    movq $0, 40(%rsp)
    call PipelineKeyAbiProbe
    xorl %eax, %eax
    addq $56, %rsp
.cfi_def_cfa_offset 8
#else
    subq $8, %rsp
.cfi_def_cfa_offset 16
    movq %rsi, %r10
    movq %rdx, %rcx
    movq %rdi, %rsi
    movq $2, %rdx
    movq 0(%r10), %rdi
    movq 8(%r10), %r8
    xorl %r9d, %r9d
    call PipelineKeyAbiProbe
    xorl %eax, %eax
    addq $8, %rsp
.cfi_def_cfa_offset 8
#endif
    ret
.cfi_endproc
#ifdef _WIN32
.seh_endproc
#else
.size PipelineKeyAbiNested, .-PipelineKeyAbiNested
#endif
#ifdef _WIN32
.section .rdata,"dr"
.p2align 4
.Lseed0:
    .quad 0x3141592653589793, 0x2718281828459045
.Lseed1:
    .quad 0x32425a2754599894, 0x2819291929469146
.Lseed2:
    .quad 0x33435b28555a9995, 0x291a2a1a2a479247
.Lseed3:
    .quad 0x34445c29565b9a96, 0x2a1b2b1b2b489348
.Lseed4:
    .quad 0x35455d2a575c9b97, 0x2b1c2c1c2c499449
.Lseed5:
    .quad 0x36465e2b585d9c98, 0x2c1d2d1d2d4a954a
.Lseed6:
    .quad 0x37475f2c595e9d99, 0x2d1e2e1e2e4b964b
.Lseed7:
    .quad 0x3848602d5a5f9e9a, 0x2e1f2f1f2f4c974c
.Lseed8:
    .quad 0x3949612e5b609f9b, 0x2f203020304d984d
.Lseed9:
    .quad 0x3a4a622f5c61a09c, 0x30213121314e994e
#else
.section .note.GNU-stack,"",@progbits
#endif
