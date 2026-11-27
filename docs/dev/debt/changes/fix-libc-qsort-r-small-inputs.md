# libc

## change [qsort_r]

- [qsort_r](../../../core/libs/prx/libc/src/FileAndHeap.cpp) (libc) follows FreeBSD 11 [qsort.c](https://github.com/freebsd/freebsd-src/blob/5ed7eb0d97ba4436218e810f61bd059acba984c2/lib/libc/stdlib/qsort.c): Bentley and McIlroy's quicksort, with insertion sort below 7 elements and after a partition pass that swaps nothing, so elements that compare equal end in FreeBSD's order and the comparator is called in the same sequence; `qsort_r` passes its thunk as the comparator's first argument. That the console's libc uses this FreeBSD code is unverified. A zero element size with 7 or more elements throws `std::invalid_argument`, where FreeBSD's partition loop never ends; a null comparator or base with two or more elements of nonzero size throws, where FreeBSD faults.
