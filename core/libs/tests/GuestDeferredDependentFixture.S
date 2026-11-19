.section .ginit,"dr"
.p2align 3
.globl __aps5_guest_initialize
__aps5_guest_initialize:
.rva DeferredStart_nid_no_patch
.long 1
.rva DeferredInitializerSlot_nid_no_patch
.long 0x44504541
.long 1
.asciz "guest_deferred_module_fixture.guest.prx"
