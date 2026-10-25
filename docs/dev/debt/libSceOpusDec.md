# libSceOpusDec technical debt

### Unknown function info

- [sceOpusDecCancel, sceOpusDecCreate, sceOpusDecDecodeFloat, sceOpusDecDecodeFloatWithPriority, sceOpusDecDecodeFloatWithPriorityAndTimeout, sceOpusDecDecodeWithPriority and sceOpusDecDecodeWithPriorityAndTimeout](../../../core/libs/prx/libSceOpusDec/Export.cpp) (libSceOpusDec) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceOpusDec.c), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim.

### Functional

- [libSceOpusDec](../../../core/libs/prx/libSceOpusDec/Export.cpp) uses bundled libopus 1.6.1 for mono/stereo signed-16 decoding at the creation rates the research lists: 8000, 12000, 16000, 24000 and 48000 Hz. The supplied module confirms the exported arguments, read-only context inputs, 640-byte state size and creation rates. Validation errors and their precedence are recovered. Console PCM equivalence, AJM failures, context ownership and termination with live decoders remain unimplemented. Packet loss concealment still throws; decoder state is kept in host registries instead of the native guest state layout.
- [sceOpusDecCtl](../../../core/libs/prx/libSceOpusDec/Export.cpp) (libSceOpusDec, also sceOpusCeltDecCtl): the behaviour of requests 4012/4013, 4028, 4029, 4031, 4034/4045, 4046/4047 and 4050/4051 is not measured on a console; their numbers match libopus's CTLs. Gain and phase inversion go to libopus, and the final range is libopus's; that the AJM decoder scales and reports them the same way is not measured. Reset checks the AJM result words, which the host decoder has none of. Request -1 returns those words and throws here, and setting 4050 to 1 throws because its effect is unknown. In-band FEC is stored but only matters for packet loss concealment, which still throws.
