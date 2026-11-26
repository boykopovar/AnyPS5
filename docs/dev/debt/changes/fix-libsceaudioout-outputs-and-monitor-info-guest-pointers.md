# libSceAudioOut

## change [sceAudioOutSysGetHdmiMonitorInfo]

- [sceAudioOutSysGetHdmiMonitorInfo](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) - native validation requires a non-null buffer and exactly 384 bytes, then accepts types 1 and 16. Mapped pointer/size/type errors are 0x80260004/06/0C. An unwritable buffer now returns 0x80260004 instead of crashing the host; that code for this input is unmeasured. The host still zeroes type 1 output and throws for type 16: native monitor descriptors and system audio settings are unavailable. Driver initialization failures and service errors are not represented.

## add Functional

- [sceAudioOutOutputs](../../../core/libs/prx/libSceAudioOut/src/AudioOut.cpp) (libSceAudioOut) - reads `num` output parameters from the caller's array before it uses them; an unreadable array returns 0x80260004, assumed from the null-array path and not measured on hardware. The rest of the behaviour is taken from shadPS4 and fpPS4, not confirmed on a PS5 title: each parameter's handle selects a port, a handle outside 1..8 or a closed port gives 0x80260003, and the frame count of the first port is returned.
