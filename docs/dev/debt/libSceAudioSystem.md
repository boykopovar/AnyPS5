# libSceAudioSystem technical debt

### Silent stubs

- [sceAudioSystemSystemResume and sceAudioSystemSystemSuspend](../../../core/libs/prx/libSceAudioSystem/Export.cpp) (libSceAudioSystem) - return 0 without touching host audio: ports open their own SDL devices per `sceAudioInOpen`, so there is no central audio server to suspend or resume on the host. The declarations have no parameters solely to resolve known imports; the signatures are unverified host-emulation fallback assumptions, not measured console behaviour
