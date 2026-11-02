# libScePsm technical debt

### Unknown function info

- [ScePsmMonoArrayAddrWithSize, ScePsmMonoArrayLength, ScePsmMonoDomainGet, ScePsmMonoFree, ScePsmMonoGcWbarrierGenericStore](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [ScePsmMonoStringChars, ScePsmMonoStringFromUtf16, ScePsmMonoStringLength, ScePsmMonoStringNew, ScePsmMonoStringNewWrapper](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [ScePsmMonoStringToUtf8, _ZN3sce3pss4core14IntHandleTable13NextIntHandleEv, _ZN3sce3pss4core8graphics14NativeGraphics18ReleaseVideoMemoryEPv, _ZN3sce3pss4core8graphics14NativeGraphics19AllocateVideoMemoryEjj, _ZN3sce3pss4core8graphics14NativeGraphics19AllocateVideoMemoryEjjPv](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN3sce3pss4core8graphics14NativeGraphics19ReleaseSystemMemoryEPv, _ZN3sce3pss4core8graphics14NativeGraphics20AllocateSystemMemoryEjj, _ZN3sce3pss4core8graphics14NativeGraphics20AllocateSystemMemoryEjjPv, _ZN3sce3pss4core9PsmObjectC1Ev, _ZN3sce3pss4core9PsmObjectC2Ev](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN3sce3pss4core9PsmObjectD0Ev, _ZN3sce3pss4core9PsmObjectD1Ev, _ZN3sce3pss4core9PsmObjectD2Ev, _ZN3sce3pss4core9threading4Cond4WaitEi, _ZN3sce3pss4core9threading4Cond6NotifyEv](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN3sce3pss4core9threading4CondC1ERNS2_5MutexEPKci, _ZN3sce3pss4core9threading4CondC2ERNS2_5MutexEPKci, _ZN3sce3pss4core9threading4CondD1Ev, _ZN3sce3pss4core9threading4CondD2Ev, _ZN3sce3pss5orbis9framework11PsmArrayObj6LengthEv](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN3sce3pss5orbis9framework11PsmArrayObj9ToPointerEv, _ZN3sce3pss5orbis9framework11PsmCallList10UnregisterEPFvPvES4_, _ZN3sce3pss5orbis9framework11PsmCallList6UpdateEv, _ZN3sce3pss5orbis9framework11PsmCallList8RegisterEPFvPvES4_, _ZN3sce3pss5orbis9framework11PsmCallListC1Ev](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN3sce3pss5orbis9framework11PsmCallListC2Ev, _ZN3sce3pss5orbis9framework12PsmEventData8SetValueERKNS_4Json5ValueE, _ZN3sce3pss5orbis9framework12PsmFramework10InitializeERKNS2_12PsmInitParamEiPPc, _ZN3sce3pss5orbis9framework12PsmFramework20RegisterInternalCallEPKNS2_12PsmCallEntryE, _ZN3sce3pss5orbis9framework12PsmFramework24RegisterPInvokeCallTableEPKNS2_15PsmLibraryEntryE](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN3sce3pss5orbis9framework12PsmFramework3RunEv, _ZN3sce3pss5orbis9framework12PsmFramework8FinalizeEv, _ZN3sce3pss5orbis9framework12PsmInitParamC1Ev, _ZN3sce3pss5orbis9framework12PsmInitParamC2Ev, _ZN3sce3pss5orbis9framework12PsmStringObj17NewAndStoreStringEPvPc](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN3sce3pss5orbis9framework12PsmStringObj6ToUtf8Ev, _ZN3sce3pss5orbis9framework12PsmStringObj8FreeUtf8EPc, _ZN3sce3pss5orbis9framework14PsmDelegateObj11FreePointerEv, _ZN3sce3pss5orbis9framework14PsmDelegateObj15AllocatePointerEv, _ZN3sce3pss5orbis9framework18PsmFrameworkCsharp21SetEnableNativeMethodEb](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN3sce3pss5orbis9framework21PsmMainThreadCallList10UnregisterEPFvPvES4_, _ZN3sce3pss5orbis9framework21PsmMainThreadCallList6UpdateEv, _ZN3sce3pss5orbis9framework21PsmMainThreadCallList8RegisterEPFvPvES4_, _ZN3sce3pss5orbis9framework23TerminatePsmEventCsharpEv, _ZN3sce3pss5orbis9framework24InitializePsmEventCsharpEv](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN3sce3pss5orbis9framework27TerminatePsmFrameworkCsharpEv, _ZN3sce3pss5orbis9framework28InitializePsmFrameworkCsharpEv, _ZN3sce3pss5orbis9framework8PsmEvent10UnregisterEiPFvRKNS2_12PsmEventDataEPvES7_, _ZN3sce3pss5orbis9framework8PsmEvent17RegisterDelegatorEPFvRKNS2_12PsmEventDataEPvES7_, _ZN3sce3pss5orbis9framework8PsmEvent19UnregisterDelegatorEPFvRKNS2_12PsmEventDataEPvES7_](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [_ZN3sce3pss5orbis9framework8PsmEvent4PostERKNS2_12PsmEventDataE, _ZN3sce3pss5orbis9framework8PsmEvent4SendERKNS2_12PsmEventDataE, _ZN3sce3pss5orbis9framework8PsmEvent8RegisterEiPFvRKNS2_12PsmEventDataEPvES7_, scePsmRegisterInternalCall, scePsmRegisterPInvokeCallTable](../../../core/libs/prx/libScePsm/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePsm.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libScePsm/Export.cpp) - 53 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Signatures and behavior are unverified; all declarations throw. Unknown names use literal NID aliases. Three data exports in the 3.20 image are outside this callable inventory and require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `-ndbidpphoc` |
| `Unknown` | `0PIOM2UIUpQ` |
| `Unknown` | `0Tit25nFAP4` |
| `Unknown` | `12ZHf6Z7WLg` |
| `Unknown` | `1Z9IoxZ508g` |
| `Unknown` | `5DwG5treG0o` |
| `Unknown` | `5dDsI2vg6dc` |
| `Unknown` | `6FPQyIqTJi0` |
| `Unknown` | `74+2xQDzd-c` |
| `Unknown` | `8wWLk0PaYGQ` |
| `Unknown` | `BaQ59CqSB34` |
| `Unknown` | `C08zEeeiTBI` |
| `Unknown` | `CN90Qhgm32U` |
| `Unknown` | `Ia3-a5X02RA` |
| `Unknown` | `IkQNwXle96Q` |
| `Unknown` | `IyRVF5CAG48` |
| `Unknown` | `KR9fL93LcnU` |
| `Unknown` | `L4SntCPTllw` |
| `Unknown` | `N0+F2yO3hRc` |
| `Unknown` | `OJ+3wLrf7Yk` |
| `Unknown` | `OVqKOEWE-yc` |
| `Unknown` | `PGLOWFhZTtM` |
| `Unknown` | `PN1KQl3wnoU` |
| `Unknown` | `QXGdhj5nfpM` |
| `Unknown` | `SpdSLM7ONAk` |
| `Unknown` | `UDGJZZ9jrCE` |
| `Unknown` | `UFkg2lqxKPQ` |
| `Unknown` | `W8PzsJgkCGU` |
| `Unknown` | `WAKyoyoA8Dw` |
| `Unknown` | `XmlFvKZlm7k` |
| `Unknown` | `YTcYAXVAW+A` |
| `Unknown` | `YnZFBPDyB2A` |
| `Unknown` | `bGtKORAU2eE` |
| `Unknown` | `c-fS4SiII1Q` |
| `Unknown` | `cb-Q6dnF1Uk` |
| `Unknown` | `dsbtKMQDhjM` |
| `Unknown` | `e0WJestaOok` |
| `Unknown` | `gW4wqd1GV8E` |
| `Unknown` | `iKBWRnPccBs` |
| `Unknown` | `j0tmOKH875I` |
| `Unknown` | `jU+doPzvPiQ` |
| `Unknown` | `jcyBwXYskmc` |
| `Unknown` | `m7QCwDRxED4` |
| `Unknown` | `ni7QHsx30nw` |
| `Unknown` | `owrMSr1b45c` |
| `Unknown` | `t3a4oAqrvrI` |
| `Unknown` | `tFmZ8R28vsE` |
| `Unknown` | `th1-yYnLsw0` |
| `Unknown` | `udthK0YXar0` |
| `Unknown` | `vClC0x57Nqs` |
| `Unknown` | `wDEygcO128w` |
| `Unknown` | `xzd24XYJpb0` |
| `Unknown` | `yemgMfc2oW8` |
