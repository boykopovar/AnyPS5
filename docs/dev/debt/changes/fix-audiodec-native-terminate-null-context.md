# libSceAudiodec.native

## add Unknown function info

- [sceAudiodecTerminate](../../../core/libs/prx/libSceAudiodec.native/Export.cpp) (libSceAudiodec.native) - firmware 13.42 disassembly shows a null context returns before any service call and without writing the return register, so the console return value is unknown; the host shim returns 0 as a deterministic fallback. A non-null context still reaches the unimplemented lock-destroy and Ajmi/Ajm finalization calls.
