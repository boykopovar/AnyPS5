# libSceNgs2.native

## add Functional

- [sceNgs2VoiceRunCommands batch limit and sceNgs2VoiceGetMatrixInfo guest copy](../../../core/libs/prx/libSceNgs2.native/src/Voice.cpp) (libSceNgs2.native) - the firmware command batch limit above normal use and invalid-address precedence in `sceNgs2VoiceGetMatrixInfo` remain unmeasured on a console. As a conservative host-safety cap, `sceNgs2VoiceRunCommands` rejects `num_commands > 4096` (`MAX_VOICE_COMMANDS`, `APS5_INVALID_ARG_EX`) before allocating `localCommands` or multiplying byte counts, and `sceNgs2VoiceGetMatrixInfo` copies a local `Ngs2VoiceMatrixInfo` out via `WriteGuestBytes` instead of forming guest member pointers.
