# libSceOpusDec technical debt

### Additional callable exports

- [sceOpusDecGetAjmContext](../../../core/libs/prx/libSceOpusDec/Export.cpp) hashes to `OyDFNZOLrp0`; the public 3.20 catalogue does not list it. Two version-labelled images contain function records `OyDFNZOLrp0#D#A` and `OyDFNZOLrp0#E#A`, respectively. Dynamic metadata maps those library IDs and module ID 0 to `libSceOpusDec`. Older module words: `0x08540001`, `0x03200004`; later: `0x13590001`, `0x13600007`. Their 15 and 30 function NIDs have sorted-LF SHA-256 values `f4a21bfa5759129658496cfc706b1aa43d45e5c38f04838e964b09dc97e37cbf` and `323309a804d747d76ac694770153c20644f7d30b210ca7eaaf6c15f949e05837`, using the [static ELF membership procedure](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298). Labels do not authenticate retail provenance; the digests identify NID sets only. Independent verification requires matching images. The signature and behavior are unverified; this declaration throws.

### Unknown function info

- [sceOpusDecCancel, sceOpusDecCreate and sceOpusDecCtl](../../../core/libs/prx/libSceOpusDec/Export.cpp) (libSceOpusDec) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceOpusDec.c), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim.
- [sceOpusDecDecodeWithPriorityAndTimeout and sceOpusDecDecodeFloatWithPriorityAndTimeout](../../../core/libs/prx/libSceOpusDec/Export.cpp) (libSceOpusDec) decode synchronously only for the infinite timeout 0xFFFFFFFF, the value sceOpusDecDecode passes, so the batch ID output is never written. libSceOpusDec.sprx (firmware 13.60, read from disassembly, not measured on a console) passes the timeout to its AJM batch wait; when the wait does not finish it stores the batch ID and returns 0x81740015. Whether a finite timeout finishes depends on the console's decode time, so any other timeout returns the argument errors and then throws. The priority (20 to 100) is validated but does not schedule anything

### Functional

- [libSceOpusDec](../../../core/libs/prx/libSceOpusDec/Export.cpp) uses bundled libopus 1.6.1 for mono/stereo signed-16 decoding at the creation rates the research lists: 8000, 12000, 16000, 24000 and 48000 Hz. The supplied module confirms the exported arguments, read-only context inputs, 640-byte state size and creation rates. Validation errors and their precedence are recovered. Console PCM equivalence, AJM failures, context ownership and termination with live decoders remain unimplemented. Packet loss concealment still throws; decoder state is kept in host registries instead of the native guest state layout.
