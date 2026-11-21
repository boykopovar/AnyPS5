# libSceAudioOut

## change [AudioOut2 user guest outputs]

- [AudioOut2 user guest outputs](../../../core/libs/prx/libSceAudioOut/src/AudioOut2User.cpp) - the console's behavior for a non-null but unwritable user-handle or attribute output is unmeasured. AnyPS5 returns its existing invalid-argument codes as a host-safety fallback, keeps the next handle available if creation cannot write it, and preserves host `errno`. When both attribute outputs are non-null, `sceAudioOut2UserGetSupportedAttributes` snapshots the original `contextAttributes` value before writing and restores it if writing `portAttributes` fails, avoiding partial caller-memory modification on error.
