# libc

## change [flock]

- [flock](../../../core/libs/prx/libc/src/FileLock.cpp) (libkernel) on Windows is not verified against console behavior. FreeBSD serves lock waiters in arrival order, while host `LockFileEx` uses protocol bytes for the lock, gate, queued writers and readers. The reader byte distinguishes a sole shared holder from a peer; a blocking sole-holder upgrade drains earlier queued writers through the gate, while `LOCK_NB` never waits on the gate or waiter drain and reports contention instead. Downgrade behavior relies on a shared lock over the handle's own exclusive lock succeeding before the exclusive unlock, as documented by [`LockFileEx`](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex) for same-handle shared-over-exclusive overlap (which requires two unlock operations); equivalent console behavior remains unverified. Directory locks remain process-local. Signals and closing a descriptor during a pending lock call are unverified.
