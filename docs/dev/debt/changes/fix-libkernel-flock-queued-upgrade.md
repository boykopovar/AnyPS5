# libc

## change [flock]

- [flock](../../../core/libs/prx/libc/src/FileLock.cpp) (libkernel) on Windows is not verified against console behavior. FreeBSD serves lock waiters in arrival order, while host `LockFileEx` uses protocol bytes for the lock, gate, queued writers and readers. The reader byte distinguishes a sole shared holder from a peer; a blocking sole-holder upgrade drains earlier queued writers through the gate, while `LOCK_NB` never waits on the gate or waiter drain and reports contention instead. Downgrade behavior relies on a shared lock over the handle's own exclusive lock succeeding before the exclusive unlock, as measured on Windows 11 but not documented. Directory locks remain process-local. Signals and closing a descriptor during a pending lock call are unverified.
