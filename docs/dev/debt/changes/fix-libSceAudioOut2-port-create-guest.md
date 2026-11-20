# libSceAudioOut

## add Unknown function info

- [sceAudioOut2PortCreate guest pointers](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Port.cpp) (libSceAudioOut) - the parameter block is copied in and the port handle copied out with `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch`, preserving the host `errno`. An unreadable block or an unwritable handle slot returns `0x80260502` (`INVALID_ARGUMENT`) instead of faulting the host, and a failed handle write releases the slot it allocated; that fallback is unmeasured on a console. Readable parameters and a writable slot answer exactly as before.
