# libSceNgs2.native

## add Unknown function info

- [sceNgs2RackGetInfo, sceNgs2SystemQueryBufferSize, sceNgs2SystemRender, sceNgs2VoiceRunCommands, sceNgs2VoiceGetMatrixInfo](../../../core/libs/prx/libSceNgs2.native/src/Rack.cpp) (libSceNgs2.native) - safely probe and copy guest memory for rack information, system buffer query outputs, render buffer descriptors, voice command batches and matrix level structures using ReadGuestBytes and WriteGuestBytes rather than dereferencing guest pointers directly.
