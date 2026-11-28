# libkernel

## add Functional

- [Thread objects](../../../core/libs/prx/libkernel/Pthread/src/Thread.cpp) (libkernel) - the objects whose addresses `scePthreadSelf`, `scePthreadCreate` and `pthread_self` give the title come from the guest heap, so they lie in the guest address range below 2^40 that titles assume: CUSA00900 keeps `scePthreadSelf()` in the low 40 bits of its adaptive mutex word and compares it with the full value. Where the console's libkernel places these objects is not measured.
