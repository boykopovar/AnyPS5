# libSceVoice

## add Unknown function info

- [sceVoiceGetPortAttr](../../../core/libs/prx/libSceVoice/Export.cpp) (libSceVoice) - copies out device attribute value via WriteGuest (GuestCopyOut_nid_no_patch) with caller errno preserved; returning SCE_VOICE_ERROR_INVALID_ARGUMENT for an unmapped or unwritable pointer is a host-safety fallback with console behavior unmeasured.
