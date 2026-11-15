# libSceAudioOut

## change [AudioOut2 user guest outputs]

- [AudioOut2 user guest outputs](../../../core/libs/prx/libSceAudioOut/src/AudioOut2User.cpp) - the console's behavior for a non-null but unwritable user-handle or attribute output is unmeasured. AnyPS5 returns its existing invalid-argument codes as a host-safety fallback, keeps the next handle available if creation cannot write it, and preserves host `errno`. When only the second attribute output is unwritable, the first output is restored to its prior value before returning.
