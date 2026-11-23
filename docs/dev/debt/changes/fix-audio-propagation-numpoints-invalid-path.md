# libSceAudioPropagation

## change [sceAudioPropagationSourceGetAudioPath]

- [sceAudioPropagationSourceGetAudioPath](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) (libSceAudioPropagation) - returning `ErrorNullPointer` (`0x8A700003`) when the `outPath` pointer is unwritable via `GuestCopyOut_nid_no_patch` is a host-safety fallback preserving host errno. On a payload process (firmware 11.60) an unknown source handle (0 and -1) returned `ErrorInvalidHandle` (`0x8A700002`) before the `outPath` pointer was read, also for a null and an unmapped (`0x10`) `outPath`; console behavior on unmapped pointers for a live source is not measured. Destroyed source handles are unmeasured, the same lookup is assumed.

## change [sceAudioPropagationPathGetNumPoints]

- [sceAudioPropagationPathGetNumPoints](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) (libSceAudioPropagation) - returning `ErrorNullPointer` (`0x8A700003`) when the `count` pointer is unwritable via `GuestCopyOut_nid_no_patch` is a host-safety fallback preserving host errno. On a payload process (firmware 11.60) an unknown path handle (0 and -1) returned `ErrorInvalidHandle` (`0x8A700002`) before the `count` pointer was read, also for a null and an unmapped (`0x10`) `count`, and so does this, and for a destroyed path the same lookup is assumed (unmeasured); a handle of another kind of object still throws here and was not measured, and console behavior on unmapped pointers for a live path is not measured.
