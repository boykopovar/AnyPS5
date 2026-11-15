# libSceAudiodec.native

## add Unknown function info

- [sceAudiodecCreateDecoderEx guest input fallback](../../../core/libs/prx/libSceAudiodec.native/Export.cpp) (libSceAudiodec.native) - `ctrl`, `ctrl->pParam`, and `ctrl->pBsiInfo` size-header reads use `GuestCopyIn_nid_no_patch` and return `0x807F0008` (`AudiodecErrorInvalidCtrlPointer`), `0x807F0009` (`AudiodecErrorInvalidParamPointer`), and `0x807F000A` (`AudiodecErrorInvalidBsiInfoPointer`) with caller `errno` preserved on unreadable guest pointers as a host-safety fallback; console behavior on unmapped pointers was not measured.
