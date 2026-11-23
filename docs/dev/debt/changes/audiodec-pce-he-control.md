# libSceAudiodec

## change an ADTS stream with channel configuration 0

- [sceAudiodecDecode](../../../core/libs/prx/libSceAudiodec/src/Codecs.cpp) (libSceAudiodec) - PCE control behavior is not measured on a console. The existing HE policy also applies to equivalent PCE streams. Supported PCE changes must lead a single ADTS raw block without CRC; later frames can omit the unchanged configuration. An initial non-leading PCE, CRC, multiple blocks, or PCE bits that mimic an ASC sync extension throw. PCE changes elsewhere in a raw block remain unsupported. The sync limit prevents FFmpeg from reading PCE fields or comments as HE policy. Clear retains configuration and flushes the decoder; fresh HE PCM identity after clear is not established. Labels: console, tested, linux.
