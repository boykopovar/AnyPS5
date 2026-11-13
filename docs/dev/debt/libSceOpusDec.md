# libSceOpusDec technical debt

### Additional callable exports

- [sceOpusDecGetAjmContext](../../../core/libs/prx/libSceOpusDec/Export.cpp) hashes to `OyDFNZOLrp0`; the public 3.20 catalogue does not list it. Two version-labelled images contain function records `OyDFNZOLrp0#D#A` and `OyDFNZOLrp0#E#A`, respectively. Dynamic metadata maps those library IDs and module ID 0 to `libSceOpusDec`. Older module words: `0x08540001`, `0x03200004`; later: `0x13590001`, `0x13600007`. Their 15 and 30 function NIDs have sorted-LF SHA-256 values `f4a21bfa5759129658496cfc706b1aa43d45e5c38f04838e964b09dc97e37cbf` and `323309a804d747d76ac694770153c20644f7d30b210ca7eaaf6c15f949e05837`, using the [static ELF membership procedure](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298). Labels do not authenticate retail provenance; the digests identify NID sets only. Independent verification requires matching images. The signature and behavior are unverified; this declaration throws.

- [Later callable exports](../../../core/libs/prx/libSceOpusDec/Export.cpp) - the function exports of `libSceOpusDec` listed below throw through `NotImplemented_nid_no_patch` on every call; their names, parameter lists, return types and behavior are unknown, and the parameterless `int` declarations only resolve imports. The [PS5 13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/ba1933f305f1934ed2a12c94479a977844bdbe5a/data/firmware-13.60/libsceopusdec-69c895457e.jsonl) lists them as function exports of the 13.60-labelled image only, not of its 3.20 reference; the label does not authenticate retail firmware. `tools/nid_names.py` resolves none of them, so each uses a literal NID alias.

| Name | NID |
| --- | --- |
| `Unknown` | `-NcPFjTj5zU` |
| `Unknown` | `EF6FftzF33M` |
| `Unknown` | `I1ugumHd0+A` |
| `Unknown` | `JZapTsLx0j0` |
| `Unknown` | `VsujeTF0+xk` |
| `Unknown` | `ZBx02KtHz2Y` |
| `Unknown` | `ct42RpB2CaM` |
| `Unknown` | `eJQx7Emrwb0` |
| `Unknown` | `ewfq1d5iJhU` |
| `Unknown` | `lFTdHt-AuKU` |
| `Unknown` | `sr25irrwGBI` |
| `Unknown` | `ygCoF49g3F0` |

### Unknown function info

- [sceOpusDecCancel and sceOpusDecCreate](../../../core/libs/prx/libSceOpusDec/Export.cpp) (libSceOpusDec) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceOpusDec.c), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim.
- [sceOpusDecGetAjmContext, `gMazl3u8Y40`, `jXVNwfmd1II` and `ark0tPj4meI`](../../../core/libs/prx/libSceOpusDec/Export.cpp) (libSceOpusDec) - only the cases firmware 13.42's module settles without a decoder are answered; the names of the three NIDs are unknown and the parameter lists come from that module's register use. `sceOpusDecGetAjmContext` and `gMazl3u8Y40` return `0x81740000` for a null state and 0 for a null output; for a real state the module reads its own state layout (offsets 0x88 and 0x90), which the host does not keep, so they throw. `jXVNwfmd1II` returns `0x8174000d` for a null context; otherwise it initializes AJM into that context and registers the decoder module, so it throws. `ark0tPj4meI` is complete: three 32-bit values checked against 1 to 12 and a sum below 13, returning 0x290. Read from the module's disassembly, not measured on a console.
- [sceOpusDecDecodeWithPriorityAndTimeout and sceOpusDecDecodeFloatWithPriorityAndTimeout](../../../core/libs/prx/libSceOpusDec/Export.cpp) (libSceOpusDec) decode synchronously only for the infinite timeout 0xFFFFFFFF, the value sceOpusDecDecode passes, so the batch ID output is never written. libSceOpusDec.sprx (firmware 13.60, read from disassembly, not measured on a console) passes the timeout to its AJM batch wait; when the wait does not finish it stores the batch ID and returns 0x81740015. Whether a finite timeout finishes depends on the console's decode time, so any other timeout returns the argument errors and then throws. The priority (20 to 100) is validated but does not schedule anything

### Functional

- [libSceOpusDec](../../../core/libs/prx/libSceOpusDec/Export.cpp) uses bundled libopus 1.6.1 for mono/stereo signed-16 decoding at the creation rates the research lists: 8000, 12000, 16000, 24000 and 48000 Hz. The supplied module confirms the exported arguments, read-only context inputs, 640-byte state size and creation rates. Validation errors and their precedence are recovered. Console PCM equivalence, AJM failures, context ownership and termination with live decoders remain unimplemented. Packet loss concealment still throws; decoder state is kept in host registries instead of the native guest state layout.
- [sceOpusDecInitialize](../../../core/libs/prx/libSceOpusDec/include/OpusDecoder.hpp) (libSceOpusDec, shared with `sceOpusCeltDecInitialize`) - returning `0x8174000d` (`0x8140000d` for CELT) when `context` is unwritable via `GuestCopyOut_nid_no_patch` is a host-safety fallback with console behavior unmeasured.
- [sceOpusDecCtl](../../../core/libs/prx/libSceOpusDec/Export.cpp) (libSceOpusDec, also sceOpusCeltDecCtl): the effect of requests 4012/4013, 4028, 4029, 4031, 4034/4045, 4046/4047 and 4050/4051 on decoding is not measured; their numbers match libopus's CTLs. Their argument checks, error codes, initial values, stored values and reset follow libSceOpusDec.sprx and libSceOpusCeltDec.sprx (firmware 13.42, run on a PC with AJM stubbed, not measured on a console). Gain and phase inversion go to libopus, and the final range is libopus's; that the AJM decoder scales and reports them the same way is not measured. Reset checks the AJM result words, which the host decoder has none of. Request -1 returns those words: it writes 0 to both before the first decode and throws after one. 4050 set to 1 is stored until reset, but its effect is unknown, so decoding with it set throws. In-band FEC is stored but only matters for packet loss concealment, which still throws.
