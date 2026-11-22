# libc

## change [flock]

- [flock](../../../core/libs/prx/libc/src/FileLock.cpp) (libkernel) on Windows is not verified against console behavior. FreeBSD serves lock waiters in arrival order; host `LockFileEx` uses protocol bytes for the lock, gate, queued writers and readers. The reader byte identifies a sole shared holder. Blocking sole-holder upgrades drain earlier writers; nonblocking upgrades serialize on the gate but never drain waiters, while fresh `LOCK_NB` acquisitions fail immediately on contention. Downgrade relies on same-handle shared-over-exclusive overlap documented by [`LockFileEx`](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex) (two unlocks); console equivalence remains unverified. Directory locks remain process-local. Signals and closing a descriptor during a pending call are unverified.
