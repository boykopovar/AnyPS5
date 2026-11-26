# libwebrtc

## add Public source implementations

- [rtc::Thread::Quit](../../../core/libs/prx/libwebrtc/src/ThreadQuit.cpp) (libwebrtc) - console layout and dispatch are unverified. Follows [M134 thread.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/rtc_base/thread.cc), unchanged at [M127](https://webrtc.googlesource.com/src/+/e0b28a6a81a989c1f5c89e30fcd247870047390d/rtc_base/thread.cc): release-store 1 at the existing stop offset 0x64, then WakeUp on the socket server at 0x68, even on repeated calls. WakeUp slot 5 follows both revisions' socket_factory.h and socket_server.h and the existing NullSocketServer. Invalid memory, misalignment or absent callbacks throw where upstream dereferences them. Thread construction and message-loop exports remain unimplemented. Closes when: compared with the console.
