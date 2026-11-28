# relinker

## change [Windows guest GS replay]

- [Windows guest GS replay](../../../core/relinker/elfpatcher/src/windows/WindowsGuestGs.cpp) does not support precise FreeBSD signal/exception delivery, SS versus GP fault classification, debugger single stepping, external thread-context manipulation, self-modifying rewritten code or dynamic/JIT GS code. Windows frame-based exception unwinding has no guest unwind record for a replay slot. Only one replay can be active per thread, so a host exception handler must not execute another rewritten GS instruction while deciding whether to retry a replay. Single stepping into replay fails; guest exception redirection waits for replay to finish and throws on its existing one-second restoration timeout. Replay reserves 128 bytes above its exception stack for the guest red zone. No title or PS5 execution has verified this path; the six accesses in Prospero Win (PPSA99995)'s Wine ntdll do not establish a reachable failure or application unblock.
