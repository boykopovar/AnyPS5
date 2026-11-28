# libSceOpusDec

## change [sceOpusDecCreate]

- [sceOpusDecCreate](../../../core/libs/prx/libSceOpusDec/Export.cpp) (libSceOpusDec) - the supplied PRX image (SHA-256 `723999389ee02c877bccc4cdd98eb41bf5548380b3a20bf0b95e099d935343b4`; firmware version unverified) shows `(state, sampleRate, channels)`, validation in that order, then the same worker as `sceOpusDecCreateEx` using a module-wide context. This implementation reuses `OpusDecoderLibrary::CreateEx` and a private shared context; unlike firmware's load-time initialization, the host creates it lazily. The load-time context creation error path is not established.
