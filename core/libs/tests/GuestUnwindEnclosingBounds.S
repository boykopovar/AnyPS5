.cfi_sections .eh_frame
.text
.p2align 4

.globl GuestUnwindBoundsFirst
.type GuestUnwindBoundsFirst, @function
GuestUnwindBoundsFirst:
    .cfi_startproc
    .fill 15, 1, 0x90
    ret
    .cfi_endproc
.globl GuestUnwindBoundsFirstEnd
GuestUnwindBoundsFirstEnd:
.size GuestUnwindBoundsFirst, .-GuestUnwindBoundsFirst
    .fill 16, 1, 0x90

.globl GuestUnwindBoundsSecond
.type GuestUnwindBoundsSecond, @function
GuestUnwindBoundsSecond:
    .cfi_startproc
    .fill 15, 1, 0x90
    ret
    .cfi_endproc
.globl GuestUnwindBoundsSecondEnd
GuestUnwindBoundsSecondEnd:
.size GuestUnwindBoundsSecond, .-GuestUnwindBoundsSecond
    .fill 16, 1, 0x90

.section .note.GNU-stack,"",@progbits
