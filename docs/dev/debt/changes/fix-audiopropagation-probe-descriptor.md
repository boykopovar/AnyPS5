# libSceAudioPropagation

## add Unknown function info

- [Struct descriptor validation](../../../core/libs/prx/libSceAudioPropagation/Export.cpp) (libSceAudioPropagation) - `requireDescriptor` safely copies the struct descriptor header using `copyGuestIn` before validating `id` and `size`, throwing `std::invalid_argument` if the guest pointer is unreadable or invalid, because console behavior on unmapped memory is unmeasured.
