# libSceAudioOut

## add Mastering guest access

- [sceAudioOut2MasteringGetState](../../../core/libs/prx/libSceAudioOut/src/AudioOut2Mastering.cpp) - reads states_id through GuestCopyIn_nid_no_patch (ReadMasteringId) and writes the filled state through GuestCopyOut_nid_no_patch (FillMasteringState), returning SCE_AUDIO_OUT2_ERROR_INVALID_POINTER with caller errno preserved when unmapped or unwritable. Console behavior on unmapped addresses is unmeasured.
