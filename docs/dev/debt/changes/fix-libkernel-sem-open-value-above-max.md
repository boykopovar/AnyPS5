# libkernel

## change [sem_open, sem_close, sem_unlink]

- [sem_open, sem_close, sem_unlink](../../../core/libs/prx/libkernel/Pthread/Posix/Sem.cpp) (libkernel) keep named semaphores in a table inside the process; FreeBSD 11.4's [`sem_new.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/sem_new.c#L138-L308) backs each with a file `/tmp/SEMD<name>`. A semaphore lasts until `sem_unlink` and its last `sem_close` but is not shared with other processes, and the mode is ignored (no permission errors, as for a root caller). Whether a title can create files under `/tmp` on the console is unverified. A name with a further `/` throws, as it would resolve below `/tmp`. An initial value above `SEM_VALUE_MAX` keeps its low 31 bits, the count FreeBSD 11.4 reads with `USEM_COUNT` after storing it unchecked; the has-waiters bit it also sets there only adds wake-ups.
