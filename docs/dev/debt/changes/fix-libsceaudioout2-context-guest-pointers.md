# libSceAudioOut

## change [sceAudioOut2ContextQueryMemory guest output]

- [sceAudioOut2ContextQueryMemory guest output](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Context.cpp) - a non-null unwritable memory-size output previously crashed the host, as did an unreadable parameter set. The console's result for those inputs is unmeasured; AnyPS5 returns its existing invalid-argument codes as a host-safety fallback (`0x80260502` for an unreadable parameter set) and preserves host `errno`. The successful size is `61196 + 2912 * max_ports`, as `sceAudioOut2ContextQueryMemory` computes it.
