# libwebrtc

## add Public source implementations

- [rtc::Thread message loop](../../../core/libs/prx/libwebrtc/src/Thread.cpp) (libwebrtc) - console layout and dispatch are unverified. Follows [M134 thread.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/rtc_base/thread.cc) with the virtual order of thread.h and task_queue_base.h. Only the vptr, stop (0x64) and socket server (0x68) are written in the object: the queues, pthread and task queue registration live in a host table keyed by the address, so the exports that use them throw on a thread this constructor did not build. name_ is never set and the thread is not named. ThreadManager::Add and Remove are skipped: no export reads that list. PostDelayedTaskImpl throws when the rounded delay does not fit in int milliseconds, where an upstream release build truncates. absl::AnyInvocable is taken as 16 bytes of state, then manager and invoker.
