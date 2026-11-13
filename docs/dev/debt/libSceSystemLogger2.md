# libSceSystemLogger2 technical debt

### Unknown function info

- [sceSystemLogger2GetTimestamp, sceSystemLogger2Initialize, sceSystemLogger2SetEventAsync and sceSystemLogger2SetEventAsyncEx](../../../core/libs/prx/libSceSystemLogger2/Export.cpp) (libSceSystemLogger2) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSystemLogger2.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
- [sceSystemLogger2GetEventStatus, sceSystemLogger2IsAvailableMonitor, sceSystemLogger2TerminateWithResourceRelease, sceSystemLogger2SetEventAsyncEx2, sceSystemLogger2SetRnpsEventAsync, sceSystemLogger2SetEventAsyncInternal](../../../core/libs/prx/libSceSystemLogger2/Export.cpp) have unknown signatures and behaviour. The parameterless `int` declarations are import-resolution placeholders, not confirmed ABI signatures, and every call throws through `NotImplemented_nid_no_patch`. Only the names and NIDs are established by the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscesystemlogger2-2d5a872a6c.jsonl). `sceSystemLogger2SetRnpsEventAsync` is shell-facing inventory only; RNPS service behaviour and title consumption remain unverified.
- [sceSystemLogger2Terminate](../../../core/libs/prx/libSceSystemLogger2/Export.cpp) (libSceSystemLogger2) - its parameters, if any, are unknown; the no-parameter `int` declaration claims only the 32-bit return. In firmware 13.42's `libSceSystemLogger2.sprx` (SHA-256 `59df37c466716e98673285387b8e0551ee01fb931bae8add4c5574b2e02d5c34`), read from disassembly and not run on a console, it is `xor %eax,%eax; ret` at `0x1550`: it returns 0 whether or not `sceSystemLogger2Initialize` ran, reads no argument and writes no memory.

### Additional callable exports


- [Additional callable exports](../../../core/libs/prx/libSceSystemLogger2/Export.cpp) - 3 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `PcjxaaFQATU` |
| `Unknown` | `UI7zoVuRjdU` |
| `Unknown` | `eI4wpp+8Bco` |
