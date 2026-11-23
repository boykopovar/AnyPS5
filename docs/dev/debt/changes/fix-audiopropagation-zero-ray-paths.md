# libSceAudioPropagation

## change lacks acoustic propagation

- [libSceAudioPropagation](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) lacks acoustic propagation: attributes have no effect, sources expose 36 initially empty, invalid path slots and no rays, rendering preserves the destination mix without adding path contributions, and allocated CPU work memory is mostly unused. Ray results accept only an empty list and throw otherwise; path calculation has its own entry below. Room, portal, source and material handles are tracked, but destruction after their system is destroyed silently succeeds without native lifetime semantics being confirmed. Occlusion, reflections and reverb are absent.

## add Silent stubs

- [sceAudioPropagationSourceCalculateAudioPaths](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) returns 0 for no ray results, where the module returns `0x8A700001` as for every ray count but 6 (commit c0e20df906; not measured on a console). This departs from the module because this model's `sceAudioPropagationSourceGetRays` requests no rays, so a title has none to pass. The call then reports that no path was found: it checks every output's descriptor, then writes point count 0 and the invalid flag into each and leaves the rest. What the module writes for a path it does not find is not known. An output that can be read but not written throws after the earlier ones were written. Other ray counts return `0x8A700001`; 6 throws, as propagation is not implemented.

## add Unknown function info

- [sceAudioPropagationSourceSetAudioPaths](../../../core/libs/prx/libSceAudioPropagation/Export.cpp): what it does is inferred from its name and one call site, not from the module; its validation order and error codes are not known. Each entry is applied as `sceAudioPropagationSourceSetAudioPath` applies one path. Every entry is checked before any is applied, and an entry that `sceAudioPropagationSourceSetAudioPath` would reject throws. An entry naming another source's path, or a path an earlier entry names, also throws; neither was observed. An empty list is accepted, with or without a null pointer, as on `main`; whether the module accepts it is not known. The 40-byte entry layout is from PPSA21564 01.007.000 (the loop before the call at `0xf52396`): descriptor `0x010107d9` and size `0x28`, a path handle at +16, a pointer to a 224-byte path description at +24 and a float gain at +32, with +4 and +36 not written.
