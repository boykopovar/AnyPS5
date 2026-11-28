# libSceAudioPropagation

## add Functional

- [System, portal and source attribute arrays](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) (libSceAudioPropagation) - `sceAudioPropagationSystemSetAttributes`, `sceAudioPropagationPortalSetAttributes` and `sceAudioPropagationSourceSetAttributes` read the `attributes` array with one `GuestCopyIn_nid_no_patch` before checking it. An array that cannot be read throws `std::invalid_argument`, as a null array with a nonzero count does. This is a host-safety fallback; whether the console faults or returns an error for an unmapped array is unmeasured.
