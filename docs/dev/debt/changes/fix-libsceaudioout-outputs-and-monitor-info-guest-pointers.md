# libSceAudioOut

## change [sceAudioOutSysGetHdmiMonitorInfo]

- [sceAudioOutSysGetHdmiMonitorInfo](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) - native validation requires a non-null buffer and exactly 384 bytes, then accepts types 1 and 16. Mapped pointer/size/type errors are 0x80260004/06/0C. An unwritable buffer now returns 0x80260004 instead of crashing the host; that code for this input is unmeasured. The host still zeroes type 1 output and throws for type 16: native monitor descriptors and system audio settings are unavailable. Driver initialization failures and service errors are not represented.

## add Functional

- [sceAudioOutOutputs](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - reads the caller's output parameters in bounded chunks before it uses them, so an unreadable array returns 0x80260004 before its whole length is allocated, and a host allocation failure returns the same code. The console's limit on `num` is unmeasured and no reference defines one (prosper's PS5 library, shadPS4 and fpPS4 all loop over `num`), so every count is accepted; the chunked read bounds host memory by what the guest has mapped. The rest of the behaviour is taken from shadPS4 and fpPS4, not confirmed on a PS5 title: each parameter's handle selects a port, a handle outside 1..32 or a closed port gives 0x80260003, and the frame count of the first port is returned.
