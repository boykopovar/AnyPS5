# libSceWebRtc

## change Callable exports](

- [Callable exports](../../../core/libs/prx/libSceWebRtc/Export.cpp) - 5 entries are listed below. For the 4 `Unknown` entries, parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts, and each throws through `NotImplemented_nid_no_patch` with literal NID aliases and no runtime functionality. `_ZN3sce2np3rtc9TerminateEv` is covered under Functional. Data exports require separate layout and ownership analysis. Candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

## add Functional

- [_ZN3sce2np3rtc9TerminateEv](../../../core/libs/prx/libSceWebRtc/Export.cpp) (`sce::np::rtc::Terminate`, NID `-sx0eFixkek`) - follows a [reading of firmware 13.42's libSceWebRtc.sprx](https://github.com/boykopovar/AnyPS5/issues/21872#issuecomment-6946128207), read from disassembly and not measured on a console: it takes no arguments, unloads `libSceRandom` (`0x00ba`) via `sceSysmoduleUnloadModule` and `libSceRtc` (`0x80000020`), `libSceOpusDec` (`0x80000069`), `libSceOpusCeltEnc` (`0x80000043`) and `libSceOpusSilkEnc` (`0x80000068`) via `sceSysmoduleUnloadModuleInternal` (ignoring those results), and returns the result of `sceSysmoduleUnloadModuleInternal(0x80000092)` (`libSceResourceArbitrator`). In firmware 13.42 it first tears down the context at `0xdc190` when non-null; only `SRJUfO0JINA` creates that context and it still throws here, so only the null-context path runs.
