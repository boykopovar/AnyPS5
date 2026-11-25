# libSceAudioPropagation

## add Functional

- [sceAudioPropagationSystemRegisterMaterial](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) (libSceAudioPropagation) - the descriptor header is copied with `copyGuestIn` before its ID and size are read; an unreadable guest pointer throws `std::invalid_argument` as a host-safety fallback preserving `errno`. Console behavior on unmapped material descriptors is unmeasured.