#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GCC_ATOMICOPS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GCC_ATOMICOPS_HPP

inline unsigned int GccAtomicFetchAdd(volatile unsigned int* target, unsigned int value) {
    return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST);
}

inline unsigned int GccAtomicFetchSub(volatile unsigned int* target, unsigned int value) {
    return __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST);
}

#endif
