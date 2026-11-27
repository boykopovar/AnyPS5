# libSceOpusDec

## change [libSceOpusDec]

- [libSceOpusDec](../../../core/libs/prx/libSceOpusDec/Export.cpp) uses bundled libopus 1.6.1 for mono/stereo signed-16 decoding at the creation rates the research lists: 8000, 12000, 16000, 24000 and 48000 Hz. The supplied module confirms the exported arguments, read-only context inputs, 640-byte state size and creation rates. Validation errors and their precedence are recovered. Packet loss concealment calls libopus with a null packet and zero length, for the last packet's duration (20 ms before the first packet), cut to what fits in the output capacity in 2.5 ms steps; the console's duration convention and PCM equivalence are unmeasured. AJM failures, context ownership and termination with live decoders remain unimplemented. Decoder state is kept in host registries instead of the native guest state layout.

## change [sceOpusDecCtl]

- [sceOpusDecCtl](../../../core/libs/prx/libSceOpusDec/Export.cpp) (libSceOpusDec, also sceOpusCeltDecCtl): the effect of requests 4012/4013, 4028, 4029, 4031, 4034/4045, 4046/4047 and 4050/4051 on decoding is not measured; their numbers match libopus's CTLs. Their argument checks, error codes, initial values, stored values and reset follow libSceOpusDec.sprx and libSceOpusCeltDec.sprx (firmware 13.42, run on a PC with AJM stubbed, not measured on a console). Gain and phase inversion go to libopus, and the final range is libopus's; that the AJM decoder scales and reports them the same way is not measured. Reset checks the AJM result words, which the host decoder has none of. Request -1 returns those words: it writes 0 to both before the first decode and throws after one. 4050 set to 1 is stored until reset, but its effect is unknown, so decoding with it set throws. In-band FEC is stored but is not applied to a later packet.

# libSceOpusCeltDec

## change [sceOpusCeltDecDecode]

- [sceOpusCeltDecDecode](../../../core/libs/prx/libSceOpusCeltDec/Export.cpp): the original forwards packets to AJM codec 16 without a local CELT-only check. The backend response to SILK/hybrid packets is unresolved; packets whose TOC configuration is below 16 still throw. Decoding uses the bundled libopus decoder of [OpusDecoderLibrary](../../../core/libs/prx/libSceOpusDec/include/OpusDecoder.hpp) and shares its limits. Packet loss concealment uses libopus with a null packet and zero length, for the last packet's duration (20 ms before the first packet), cut to the output capacity in 2.5 ms steps; the console's duration convention is unmeasured. Asynchronous AJM execution is not implemented.
