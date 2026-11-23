# libSceAudioPropagation

## change [sceAudioPropagationSourceGetAudioPathCount]

- [sceAudioPropagationSourceGetAudioPathCount](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) (libSceAudioPropagation) - an unknown source handle returns `ErrorInvalidHandle` (`0x8A700002`) before the `count` pointer is touched, as the console does on a payload process on firmware 11.60 for the handles 0 and -1 with a valid `count` variable and for the handle 0 with a null and an unmapped (`0x10`) pointer; other unknown handle values are not measured. Returning `ErrorNullPointer` (`0x8A700003`) for a live source when the `count` pointer is unwritable via `GuestCopyOut_nid_no_patch` is a host-safety fallback preserving host errno; console behavior on unmapped pointers for a live source is not measured.
