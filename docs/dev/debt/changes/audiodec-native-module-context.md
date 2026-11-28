# libSceAudiodec.native

## change [libSceAudiodec.native pending exports]

- [libSceAudiodec.native pending exports](../../../core/libs/prx/libSceAudiodec.native/Export.cpp) - the argument checks for caller-owned context initialization and termination, codec registration, decoder creation and clearing, and the seven decode variants follow [firmware 13.42](https://github.com/boykopovar/AnyPS5/issues/19399#issuecomment-6848182372). The service calls for those paths, registration locking and the per-codec routines remain incomplete.

## add Functional

- [Module context and decoder deletion](../../../core/libs/prx/libSceAudiodec.native/Export.cpp) - the 32-byte snapshot includes both service IDs and the rwlock at offset 12 in the module context at 0xc014. The host opens the services and guest lock on first use. Firmware initializes them on load. The module uses the shared native AJM backend to finalize both IDs; it currently releases no service state. DeleteDecoderEx dispatch and its busy retry follow the [13.42 reading](https://github.com/boykopovar/AnyPS5/issues/19399#issuecomment-6848182372); only existing AJM/AJMI instances can be released. Unreadable input or unwritable output returns 0x807F0002 as a host fallback. Hardware results and busy service responses remain unmeasured.
