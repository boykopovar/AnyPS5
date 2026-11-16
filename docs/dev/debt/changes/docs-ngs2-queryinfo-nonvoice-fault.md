# libSceNgs2.native

## add Unknown function info

- [NGS2 QueryInfo on a non-voice handle](../../../core/libs/prx/libSceNgs2.native/src/Voice.cpp) (libSceNgs2.native) - on firmware 13.590.001 a payload calling `sceNgs2VoiceQueryInfo` with handle `0x1234` and info `0x4001` took SIGSEGV with that address as the fault address, before returning (one logged run in a payload process). AnyPS5 returns `0x804A8203` for an invalid voice, as in NGS2 voice validation, and does not fault; that return stays as a host-safety fallback.
