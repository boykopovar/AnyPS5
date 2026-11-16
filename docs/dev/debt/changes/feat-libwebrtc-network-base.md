# libwebrtc

## add NullSocketServer

- [NullSocketServer](../../../core/libs/prx/libwebrtc/src/NullSocketServer.cpp) has no console ABI or behavior verification. `_ZN3rtc16NullSocketServerC1Ev` constructs a 40-byte `rtc::NullSocketServer` (`SocketServer` -> `NullSocketServer`) backed by a `PthreadMutex` and `PthreadCond` (`rtc::Event`), with vtable slots for `~NullSocketServer()`, deleting destructor, `CreateSocket` (returns `nullptr`), `SetMessageQueue` (no-op), `Wait` (microsecond `TimeDelta` + `bool`, using `pthread_cond_wait` for `INT64_MAX` and `pthread_cond_reltimedwait_np` for finite positive timeouts, returning `true`), and `WakeUp`. `CreateSocket` returns `nullptr` directly without aborting on debug builds. Closes when: compared with the console.
