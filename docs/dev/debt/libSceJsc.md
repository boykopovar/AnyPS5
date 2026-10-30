# libSceJsc technical debt

### Unknown function info

- [JSString ABI and ownership](../../../core/libs/prx/libSceJsc/Export.cpp): the console JavaScriptCore version, allocation policy, opaque object layout and thread initialization remain unverified. These host objects cannot be passed to a title-supplied native JavaScriptCore module. The ten names occur in the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceJsc.c); ownership and signatures follow pinned WebKit [JSStringRef.cpp](https://github.com/WebKit/WebKit/blob/f274ef4c03ebace644c46f8ec55746b391acdcc8/Source/JavaScriptCore/API/JSStringRef.cpp). A null handle given to retain, release or the maximum-size query, a UTF-8 input above `INT32_MAX` bytes and a UTF-16 length above 2147483635, given or decoded from non-ASCII UTF-8, throw where WebKit faults. Other JavaScriptCore APIs remain unavailable.

### Functional

- [JSString conversion](../../../core/libs/prx/libSceJsc/Export.cpp): console behavior is unverified. UTF-16 storage, null/empty distinction and partial UTF-8 output follow pinned WebKit [OpaqueJSString.cpp](https://github.com/WebKit/WebKit/blob/f274ef4c03ebace644c46f8ec55746b391acdcc8/Source/JavaScriptCore/API/OpaqueJSString.cpp) and [UTF8Conversion.cpp](https://github.com/WebKit/WebKit/blob/f274ef4c03ebace644c46f8ec55746b391acdcc8/Source/WTF/wtf/unicode/UTF8Conversion.cpp). Malformed UTF-8 creates a null string, distinct from an empty string; output stops before an unpaired UTF-16 surrogate. The PS5 11.x [OpaqueJSString.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/API/OpaqueJSString.h#L49-L52) keeps the low 32 bits of a UTF-16 length of 2^32 or more, which throws here as in pinned WebKit.

### Known export names; ABI prototypes unverified

- The 150 throwing JavaScriptCore, Inspector, WTF and bmalloc exports in [Export.cpp](../../../core/libs/prx/libSceJsc/Export.cpp) are present in the PS5 3.20 `libSceJsc` export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Each is named after the symbol the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for its NID, and the NID of that name matches. Prototypes and behavior are unverified; each uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`. These declarations make no ABI or behavior claim.

### Additional callable exports

- [Additional shared older exports](../../../core/libs/prx/libSceJsc/Export.cpp): 17 unidentified function NIDs are present in both older JSC and WebKit images and in the later JSC image. The declarations throw; names, prototypes, runtime behavior and cross-module routing are unverified.

  - `1shvECo12S4`
  - `8LcWf8Gi0Zk`
  - `8R88n7E6UpI`
  - `B5h48AC6Vns`
  - `N-rP7qvkC1s`
  - `Oc8nnt7PBhQ`
  - `R36powg0yew`
  - `Ri+n8zTCrx4`
  - `SkTqPDJdrqA`
  - `SsrlB2FLx6g`
  - `VL5D22H8TAk`
  - `WjADfj0eVFs`
  - `eVnAQP7q5j8`
  - `eZM+AI68IpA`
  - `ecpJv6CchFE`
  - `fj3HITuKDPs`
  - `uF3DZ2xR1sA`

- [Additional callable exports](../../../core/libs/prx/libSceJsc/Export.cpp) - 356 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceJsc.c) and the [selected export metadata](https://gist.github.com/kyon-eth/e699987524d8485a9de9740280cfe4c1/09c89f4d64a17da12485e0eb637620ced4f6eeb2) from two version-labelled images (SHA-256 `d2a5f8a8807f23cb71170a5d3b779a57636f5a3480d92955e4255545ffe1e97e`). The labels are not authenticated. Prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality. Shared WebKit exports require separate loader analysis.

| Name | NID |
| --- | --- |
| `JSGlobalContextSetDebuggerRunLoopWithCurrentRunLoop` | `DojSTghs5jU` |
| `JSGlobalContextUnsetDebuggerRunLoop` | `rWPnHtWTIRg` |
| `JSRunLoopIterateCurrentRunLoop` | `XIAOofO0nN4` |
| `JSWeakObjectMapClear` | `x94EvR+FAZc` |
| `_ZN10Deprecated11ScriptValueC1ERKS0_` | `ykyWnXSOang` |
| `_ZN10Deprecated11ScriptValueC1ERN3JSC2VMENS1_7JSValueE` | `NwRNxHDt8jE` |
| `_ZN10Deprecated11ScriptValueC1Ev` | `OqmzSQH+gMQ` |
| `_ZN10Deprecated11ScriptValueC2ERKS0_` | `8JDGfjB7ysY` |
| `_ZN10Deprecated11ScriptValueC2ERN3JSC2VMENS1_7JSValueE` | `mbd08Y2w+gs` |
| `_ZN10Deprecated11ScriptValueC2Ev` | `tei1IrlQ7Yg` |
| `_ZN10Deprecated11ScriptValueD1Ev` | `ale+MG3sPCU` |
| `_ZN10Deprecated11ScriptValueD2Ev` | `eRd+WZ6MHqY` |
| `_ZN10Deprecated11ScriptValueaSERKS0_` | `35ORvn0jTS8` |
| `_ZN10Deprecated12ScriptObjectC1EPN3JSC14JSGlobalObjectEPNS1_8JSObjectE` | `svwNDrtDkgI` |
| `_ZN10Deprecated12ScriptObjectC2EPN3JSC14JSGlobalObjectEPNS1_8JSObjectE` | `lwZZL1IghHU` |
| `_ZN10Deprecated18ScriptFunctionCall4callEv` | `Tetq4yJ0ZO0` |
| `_ZN10Deprecated18ScriptFunctionCallC1ERKNS_12ScriptObjectERKN3WTF6StringEPFN3JSC7JSValueEPNS8_14JSGlobalObjectES9_RKNS8_8CallDataES9_RKNS8_7ArgListERNS4_8NakedPtrINS8_9ExceptionEEEE` | `a-LHP9X0jDw` |
| `_ZN10Deprecated18ScriptFunctionCallD1Ev` | `k6egh+rev0s` |
| `_ZN10Deprecated18ScriptFunctionCallD2Ev` | `mgFuLMJg7cY` |
| `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEN3JSC7JSValueE` | `b0+W39D6M9A` |
| `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEPKc` | `3U2E2PkIKxM` |
| `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentERKN3WTF6StringE` | `nGOnPCtlTvI` |
| `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb` | `ntUjLWHNhAM` |
| `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEi` | `qt8HISNT7CQ` |
| `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEj` | `kntFpAbDN0w` |
| `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEl` | `BEYVp9YbGN8` |
| `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEm` | `LEC1dK5v-Kg` |
| `_ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEx` | `Oqc2X663Vqc` |
| `_ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE` | `zs2x0c+f2hU` |
| `_ZN10Deprecated25ScriptCallArgumentHandlerC2EPN3JSC14JSGlobalObjectE` | `9ByZEM2Rtbk` |
| `_ZN10Deprecated25ScriptCallArgumentHandlerD1Ev` | `Kbexb6kz+D0` |
| `_ZN10Deprecated25ScriptCallArgumentHandlerD2Ev` | `GyFZR4d5pHg` |
| `_ZN10Deprecated25ScriptCallArgumentHandlernaEm` | `gsnLR9VxDdc` |
| `_ZN10Deprecated25ScriptCallArgumentHandlernwEm` | `HJshvcn63K4` |
| `_ZN3JSC10Identifier3addERNS_2VMEPKc` | `Yig0tJzlpwk` |
| `_ZN3JSC10JSFunction11getCallDataEPNS_6JSCellE` | `BGw928BfU6E` |
| `_ZN3JSC10JSFunction16getConstructDataEPNS_6JSCellE` | `p3EiAZKNa-s` |
| `_ZN3JSC10loadModuleEPNS_14JSGlobalObjectERKN3WTF6StringENS_7JSValueES6_` | `4iJZYN0vrwc` |
| `_ZN3JSC11RegisterSet12argumentGPRSEv` | `kLpxFPhl4Q8` |
| `_ZN3JSC11RegisterSet14stackRegistersEv` | `r0Obro5wOdM` |
| `_ZN3JSC11RegisterSet19calleeSaveRegistersEv` | `lUKa4hJWiSY` |
| `_ZN3JSC11RegisterSet21macroScratchRegistersEv` | `R1lHJegLAe8` |
| `_ZN3JSC11RegisterSet25reservedHardwareRegistersEv` | `82mYiWFCqJk` |
| `_ZN3JSC11RegisterSet7allFPRsEv` | `opIiuXX+a58` |
| `_ZN3JSC11RegisterSet7allGPRsEv` | `uW8TLDZhSV4` |
| `_ZN3JSC11SymbolTable14finishCreationERNS_2VME` | `3EUK5l6RAVk` |
| `_ZN3JSC11VMInspector9forEachVMEON3WTF8FunctionIFNS0_13FunctorStatusERNS_2VMEEEE` | `5m7AgdxA2ps` |
| `_ZN3JSC11createErrorEPNS_14JSGlobalObjectENS_7JSValueERKN3WTF6StringEPFS4_S6_S6_NS_11RuntimeTypeENS_13ErrorInstance28SourceTextWhereErrorOccurredEE` | `ahAMvg6mcGk` |
| `_ZN3JSC12RegExpObject14deletePropertyEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameERNS_18DeletePropertySlotE` | `EEUBxvVSYaE` |
| `_ZN3JSC12RegExpObject14finishCreationERNS_2VME` | `qEWEdAxoWA8` |
| `_ZN3JSC12RegExpObject17defineOwnPropertyEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERKNS_18PropertyDescriptorEb` | `bSsE1hCh2l8` |
| `_ZN3JSC12StringObject10putByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEjNS_7JSValueEb` | `i6fpxNFB7QE` |
| `_ZN3JSC12StringObject14deletePropertyEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameERNS_18DeletePropertySlotE` | `5OXhLz8erX4` |
| `_ZN3JSC12StringObject17defineOwnPropertyEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERKNS_18PropertyDescriptorEb` | `bKWbQfLpisA` |
| `_ZN3JSC12StringObject21deletePropertyByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEj` | `7IlxAwnpT4g` |
| `_ZN3JSC12StringObject25getOwnPropertySlotByIndexEPNS_8JSObjectEPNS_14JSGlobalObjectEjRNS_12PropertySlotE` | `ogTbJTx00wg` |
| `_ZN3JSC12StringObject3putEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameENS_7JSValueERNS_15PutPropertySlotE` | `K9oWUPuuLUg` |
| `_ZN3JSC12VMEntryScopeC1ERNS_2VMEPNS_14JSGlobalObjectE` | `5d986ipaDyw` |
| `_ZN3JSC12VMEntryScopeC2ERNS_2VMEPNS_14JSGlobalObjectE` | `K4igogiqprA` |
| `_ZN3JSC12VMEntryScopeD1Ev` | `rLV7ZbHNgeA` |
| `_ZN3JSC12VMEntryScopeD2Ev` | `k+0fsmsW2q8` |
| `_ZN3JSC12importModuleEPNS_14JSGlobalObjectERKNS_10IdentifierENS_7JSValueES5_` | `slkUdcMYXaI` |
| `_ZN3JSC13BooleanObject14finishCreationERNS_2VME` | `9DDCGs+YGTE` |
| `_ZN3JSC13JSFinalObject13visitChildrenEPNS_6JSCellERNS_11SlotVisitorE` | `GD26e1U9SCE` |
| `_ZN3JSC14JSGlobalObject17defineOwnPropertyEPNS_8JSObjectEPS0_NS_12PropertyNameERKNS_18PropertyDescriptorEb` | `2A508ZEMWzM` |
| `_ZN3JSC14JSGlobalObject18getOwnPropertySlotEPNS_8JSObjectEPS0_NS_12PropertyNameERNS_12PropertySlotE` | `V8fi8R72O2k` |
| `_ZN3JSC14JSGlobalObject25setRemoteDebuggingEnabledEb` | `BFP+XuuORYo` |
| `_ZN3JSC14JSGlobalObject30deprecatedCallFrameForDebuggerEv` | `nZc-yznH5Gw` |
| `_ZN3JSC14JSGlobalObject3putEPNS_6JSCellEPS0_NS_12PropertyNameENS_7JSValueERNS_15PutPropertySlotE` | `SxbBlpmAx1I` |
| `_ZN3JSC14JSGlobalObject7destroyEPNS_6JSCellE` | `O-QWnOr1bH8` |
| `_ZN3JSC14JSRunLoopTimer13timeUntilFireEv` | `Nqx7xm6ZKm0` |
| `_ZN3JSC14LocalAllocator16allocateSlowCaseERNS_4HeapEPNS_17GCDeferralContextENS_21AllocationFailureModeE` | `wX3ZGWU6Rf4` |
| `_ZN3JSC14ProtoCallFrame14needArityCheckEv` | `R4cvOCertOM` |
| `_ZN3JSC14ProtoCallFrame4initEPNS_9CodeBlockEPNS_14JSGlobalObjectEPNS_8JSObjectENS_7JSValueEiPS7_` | `tVCtmnhUdkg` |
| `_ZN3JSC14StructureCache32emptyObjectStructureConcurrentlyEPNS_14JSGlobalObjectEPNS_8JSObjectEj` | `AEqxJxKr8BI` |
| `_ZN3JSC14formatDateTimeERKN3WTF17GregorianDateTimeENS_14DateTimeFormatEb` | `r7U2ThCt1ag` |
| `_ZN3JSC15ArrayBufferViewD0Ev` | `MjxA6y7tetw` |
| `_ZN3JSC15ArrayBufferViewD1Ev` | `DYsGuyxpPUE` |
| `_ZN3JSC15ArrayBufferViewD2Ev` | `iYCgf+sk0iw` |
| `_ZN3JSC15JSWeakObjectRef14finishCreationERNS_2VMEPNS_8JSObjectE` | `3WxN4u0Qywg` |
| `_ZN3JSC15JSWrapperObject13visitChildrenEPNS_6JSCellERNS_11SlotVisitorE` | `no-jO-qaBAI` |
| `_ZN3JSC16InternalFunction11getCallDataEPNS_6JSCellE` | `8FBD-Rkv4hQ` |
| `_ZN3JSC16InternalFunction13visitChildrenEPNS_6JSCellERNS_11SlotVisitorE` | `18gO8FIVDOg` |
| `_ZN3JSC16InternalFunction16getConstructDataEPNS_6JSCellE` | `DJktOWakWpE` |
| `_ZN3JSC16InternalFunctionC2ERNS_2VMEPNS_9StructureENS_14NativeFunctionES5_` | `XA+MKRXav1U` |
| `_ZN3JSC16IsoSubspacePerVM5forVMERNS_2VME` | `wQP33i9qQPI` |
| `_ZN3JSC16IsoSubspacePerVMC1EN3WTF8FunctionIFNS0_18SubspaceParametersERNS_2VMEEEE` | `7HOzNJVbN0g` |
| `_ZN3JSC16IsoSubspacePerVMC2EN3WTF8FunctionIFNS0_18SubspaceParametersERNS_2VMEEEE` | `cqbMRVG1FmQ` |
| `_ZN3JSC17DebuggerCallFrame5scopeEv` | `D2HdGEYkNkg` |
| `_ZN3JSC17createJSMicrotaskERNS_2VMENS_7JSValueE` | `8i0WEPLBJmI` |
| `_ZN3JSC17weakClearSlowCaseERPNS_8WeakImplE` | `CXgp9dPHOTY` |
| `_ZN3JSC19ArrayBufferContents5clearEv` | `W2xTZFf+RzE` |
| `_ZN3JSC19ArrayBufferContentsC1EOS0_` | `oxbJwN+R0zY` |
| `_ZN3JSC19ArrayBufferContentsC1Ev` | `NLBl+ZxCYGU` |
| `_ZN3JSC19ArrayBufferContentsC2EOS0_` | `NN6kXDW5Y5o` |
| `_ZN3JSC19ArrayBufferContentsC2Ev` | `yZJAEU9jvAE` |
| `_ZN3JSC19ArrayBufferContentsD1Ev` | `DMoAIt-s9Ks` |
| `_ZN3JSC19ArrayBufferContentsD2Ev` | `ZmbGnRlFl18` |
| `_ZN3JSC19ArrayBufferContentsaSEOS0_` | `CqjOWM+bC4E` |
| `_ZN3JSC19JSSymbolTableObject14deletePropertyEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameERNS_18DeletePropertySlotE` | `5uy3POCFlAc` |
| `_ZN3JSC20speculationFromValueENS_7JSValueE` | `K9Mk0uRkFII` |
| `_ZN3JSC23JSModuleNamespaceObject10putByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEjNS_7JSValueEb` | `lNicIEursI4` |
| `_ZN3JSC23JSModuleNamespaceObject14deletePropertyEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameERNS_18DeletePropertySlotE` | `wfMAwMqO7ew` |
| `_ZN3JSC23JSModuleNamespaceObject14finishCreationEPNS_14JSGlobalObjectEPNS_20AbstractModuleRecordEON3WTF6VectorISt4pairINS_10IdentifierENS3_10ResolutionEELm0ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEE` | `MK8aFRu0E2A` |
| `_ZN3JSC23JSModuleNamespaceObject17defineOwnPropertyEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERKNS_18PropertyDescriptorEb` | `24sBys581sg` |
| `_ZN3JSC23JSModuleNamespaceObject18getOwnPropertySlotEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERNS_12PropertySlotE` | `JOeNO1OiWjQ` |
| `_ZN3JSC23JSModuleNamespaceObject25getOwnPropertySlotByIndexEPNS_8JSObjectEPNS_14JSGlobalObjectEjRNS_12PropertySlotE` | `v7FyrPDEGIk` |
| `_ZN3JSC23JSModuleNamespaceObject3putEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameENS_7JSValueERNS_15PutPropertySlotE` | `3-jrvyLGA9U` |
| `_ZN3JSC23createBuiltinExecutableERNS_2VMERKNS_10SourceCodeERKNS_10IdentifierENS_15ConstructorKindENS_16ConstructAbilityE` | `nLAwO5TMgwM` |
| `_ZN3JSC23errorMesasgeForTransferEPNS_11ArrayBufferE` | `PGtPAOHyWqk` |
| `_ZN3JSC23objectProtoFuncToStringEPNS_14JSGlobalObjectEPNS_9CallFrameE` | `eUw-w9kwIhg` |
| `_ZN3JSC25JSSegmentedVariableObject13visitChildrenEPNS_6JSCellERNS_11SlotVisitorE` | `p65DgqcLlx8` |
| `_ZN3JSC25MacroAssemblerCodeRefBase11disassemblyENS_21MacroAssemblerCodePtrILN3WTF6PtrTagE3123EEEm` | `lX2it4BGiTc` |
| `_ZN3JSC27objectPrivateFuncInstanceOfEPNS_14JSGlobalObjectEPNS_9CallFrameE` | `GZKYKdhWW8A` |
| `_ZN3JSC29callHostFunctionAsConstructorEPNS_14JSGlobalObjectEPNS_9CallFrameE` | `sutMXpKgCr0` |
| `_ZN3JSC2VM15dumpRegExpTraceEv` | `4tfuolBZ-fw` |
| `_ZN3JSC41DeferredStructureTransitionWatchpointFireC1ERNS_2VMEPNS_9StructureE` | `wIRtYc0hrWM` |
| `_ZN3JSC41DeferredStructureTransitionWatchpointFireC2ERNS_2VMEPNS_9StructureE` | `KddNzNAyySc` |
| `_ZN3JSC41DeferredStructureTransitionWatchpointFireD0Ev` | `BAiD5liOBlk` |
| `_ZN3JSC41DeferredStructureTransitionWatchpointFireD1Ev` | `V9rB2CzLQbE` |
| `_ZN3JSC41DeferredStructureTransitionWatchpointFireD2Ev` | `BplzxzYzc64` |
| `_ZN3JSC4Heap16objectTypeCountsEv` | `ShbGcHuwSMk` |
| `_ZN3JSC4Heap20addMarkingConstraintESt10unique_ptrINS_17MarkingConstraintESt14default_deleteIS2_EE` | `WN26C5qp7ZY` |
| `_ZN3JSC4Heap25protectedObjectTypeCountsEv` | `4gT52GwiD0A` |
| `_ZN3JSC4Yarr10parseFlagsEN3WTF10StringViewE` | `5mlTo8nbdYU` |
| `_ZN3JSC4Yarr11YarrPatternC1ERKN3WTF6StringENS2_9OptionSetINS0_5FlagsEEERNS0_9ErrorCodeE` | `g7Id6p6fdis` |
| `_ZN3JSC4Yarr11YarrPatternC2ERKN3WTF6StringENS2_9OptionSetINS0_5FlagsEEERNS0_9ErrorCodeE` | `7YAFidwFyxs` |
| `_ZN3JSC4Yarr11byteCompileERNS0_11YarrPatternEPN3WTF20BumpPointerAllocatorERNS0_9ErrorCodeEPNS3_4LockE` | `AHTpMEo172c` |
| `_ZN3JSC4Yarr17RegularExpressionC1ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE` | `wrn8dpfNGYw` |
| `_ZN3JSC4Yarr17RegularExpressionC2ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE` | `JpNonrEoHEI` |
| `_ZN3JSC4Yarr20unicodeMatchPropertyEN3WTF6StringE` | `sxI9MUSKmsM` |
| `_ZN3JSC4Yarr25unicodeMatchPropertyValueEN3WTF6StringES2_` | `cYr5gmGlS+s` |
| `_ZN3JSC4Yarr7replaceERN3WTF6StringERKNS0_17RegularExpressionERKS2_` | `UeihRnD6hks` |
| `_ZN3JSC4Yarr9interpretEPNS0_15BytecodePatternERKN3WTF6StringEjPj` | `M8LF3jjsskg` |
| `_ZN3JSC4callEPNS_14JSGlobalObjectENS_7JSValueERKNS_7ArgListEPKc` | `gDCWqaQENaw` |
| `_ZN3JSC4callEPNS_14JSGlobalObjectENS_7JSValueERKNS_8CallDataES2_RKNS_7ArgListERN3WTF8NakedPtrINS_9ExceptionEEE` | `GrQSJl-p+c0` |
| `_ZN3JSC4callEPNS_14JSGlobalObjectENS_7JSValueES2_RKNS_7ArgListEPKc` | `B2zNei739Gs` |
| `_ZN3JSC6JSCell11analyzeHeapEPS0_RNS_12HeapAnalyzerE` | `NwUvd0AYGDI` |
| `_ZN3JSC6JSCell11getCallDataEPS0_` | `JeH+i51Qf2I` |
| `_ZN3JSC6JSCell12dumpToStreamEPKS0_RN3WTF11PrintStreamE` | `13Vdz0yQpSw` |
| `_ZN3JSC6JSCell13estimatedSizeEPS0_RNS_2VME` | `g7U6yKITcyw` |
| `_ZN3JSC6JSCell16getConstructDataEPS0_` | `9gXDUNzVOiM` |
| `_ZN3JSC6JSCell17customHasInstanceEPNS_8JSObjectEPNS_14JSGlobalObjectENS_7JSValueE` | `C7KVLWk-IVk` |
| `_ZN3JSC6JSCell7destroyEPS0_` | `85S6tE7Yj9Q` |
| `_ZN3JSC6RegExp12dumpToStreamEPKNS_6JSCellERN3WTF11PrintStreamE` | `6Zl8YABX-mc` |
| `_ZN3JSC7JSArray17defineOwnPropertyEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERKNS_18PropertyDescriptorEb` | `TffOsjfNe+U` |
| `_ZN3JSC7JSArray18getOwnPropertySlotEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERNS_12PropertySlotE` | `beI6iLGELZA` |
| `_ZN3JSC7JSProxy10putByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEjNS_7JSValueEb` | `RvaVAKD9xD4` |
| `_ZN3JSC7JSProxy12getPrototypeEPNS_8JSObjectEPNS_14JSGlobalObjectE` | `nM81-nqbH14` |
| `_ZN3JSC7JSProxy12isExtensibleEPNS_8JSObjectEPNS_14JSGlobalObjectE` | `8BOJm7jP1i0` |
| `_ZN3JSC7JSProxy12setPrototypeEPNS_8JSObjectEPNS_14JSGlobalObjectENS_7JSValueEb` | `q0er6lvq1sc` |
| `_ZN3JSC7JSProxy14deletePropertyEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameERNS_18DeletePropertySlotE` | `FJQLG5YKbmc` |
| `_ZN3JSC7JSProxy17defineOwnPropertyEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERKNS_18PropertyDescriptorEb` | `vIcFdyzHx-g` |
| `_ZN3JSC7JSProxy17preventExtensionsEPNS_8JSObjectEPNS_14JSGlobalObjectE` | `8OlhS07zQ6w` |
| `_ZN3JSC7JSProxy18getOwnPropertySlotEPNS_8JSObjectEPNS_14JSGlobalObjectENS_12PropertyNameERNS_12PropertySlotE` | `XhCe6wdkqbE` |
| `_ZN3JSC7JSProxy21deletePropertyByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEj` | `oF-nUtAzA6o` |
| `_ZN3JSC7JSProxy25getOwnPropertySlotByIndexEPNS_8JSObjectEPNS_14JSGlobalObjectEjRNS_12PropertySlotE` | `h7j-V++4V88` |
| `_ZN3JSC7JSProxy3putEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameENS_7JSValueERNS_15PutPropertySlotE` | `dC9lut-BufE` |
| `_ZN3JSC7JSScope6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE` | `45yMcIUmYUQ` |
| `_ZN3JSC7Options14dumpAllOptionsEP7__sFILENS0_9DumpLevelEPKc` | `pd0OULBpdtQ` |
| `_ZN3JSC7Options9setOptionEPKc` | `PZZ887t9Ia4` |
| `_ZN3JSC8FreeList14initializeBumpEPcj` | `4FTNJKyJYaE` |
| `_ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj` | `Q2lC2jg+CZk` |
| `_ZN3JSC8JSObject10putByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEjNS_7JSValueEb` | `S6FZEnyltoU` |
| `_ZN3JSC8JSObject11analyzeHeapEPNS_6JSCellERNS_12HeapAnalyzerE` | `EqL+-BktuAA` |
| `_ZN3JSC8JSObject12isExtensibleEPS0_PNS_14JSGlobalObjectE` | `rKaPnExi-70` |
| `_ZN3JSC8JSObject13estimatedSizeEPNS_6JSCellERNS_2VME` | `UpnyuFw6xnY` |
| `_ZN3JSC8JSObject13visitChildrenEPNS_6JSCellERNS_11SlotVisitorE` | `mhfsMd287UI` |
| `_ZN3JSC8JSObject14deletePropertyEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameERNS_18DeletePropertySlotE` | `4aH0-tkW5PU` |
| `_ZN3JSC8JSObject17defineOwnPropertyEPS0_PNS_14JSGlobalObjectENS_12PropertyNameERKNS_18PropertyDescriptorEb` | `M7-CqYy9wAE` |
| `_ZN3JSC8JSObject17preventExtensionsEPS0_PNS_14JSGlobalObjectE` | `vL0xXrIPKA4` |
| `_ZN3JSC8JSObject21deletePropertyByIndexEPNS_6JSCellEPNS_14JSGlobalObjectEj` | `SVi-taLrQAU` |
| `_ZN3JSC8JSObject23putDirectNativeFunctionERNS_2VMEPNS_14JSGlobalObjectERKNS_12PropertyNameEjNS_14NativeFunctionENS_9IntrinsicEPKNS_6DOMJIT9SignatureEj` | `j8DwX36HWKY` |
| `_ZN3JSC8JSObject23putDirectNativeFunctionERNS_2VMEPNS_14JSGlobalObjectERKNS_12PropertyNameEjNS_14NativeFunctionENS_9IntrinsicEj` | `QczaT3pGqI4` |
| `_ZN3JSC8JSObject25getOwnPropertySlotByIndexEPS0_PNS_14JSGlobalObjectEjRNS_12PropertySlotE` | `5ezDlX4hzec` |
| `_ZN3JSC8JSObject30putDirectNativeIntrinsicGetterERNS_2VMEPNS_14JSGlobalObjectENS_10IdentifierENS_14NativeFunctionENS_9IntrinsicEj` | `Y8SwlOdZ50c` |
| `_ZN3JSC8JSObject3putEPNS_6JSCellEPNS_14JSGlobalObjectENS_12PropertyNameENS_7JSValueERNS_15PutPropertySlotE` | `EFlL7Ps6qq0` |
| `_ZN3JSC8JSObject40putDirectNativeFunctionWithoutTransitionERNS_2VMEPNS_14JSGlobalObjectERKNS_12PropertyNameEjNS_14NativeFunctionENS_9IntrinsicEj` | `asZHW6y80xo` |
| `_ZN3JSC8JSObject47putDirectNativeIntrinsicGetterWithoutTransitionERNS_2VMEPNS_14JSGlobalObjectENS_10IdentifierENS_14NativeFunctionENS_9IntrinsicEj` | `rYAqeGlKweY` |
| `_ZN3JSC8JSObject6toThisEPNS_6JSCellEPNS_14JSGlobalObjectENS_8ECMAModeE` | `rRjtwYGnsas` |
| `_ZN3JSC8evaluateEPNS_14JSGlobalObjectERKNS_10SourceCodeENS_7JSValueERN3WTF8NakedPtrINS_9ExceptionEEE` | `l-vndn4cD2g` |
| `_ZN3JSC9CallFrame15isAnyWasmCalleeEv` | `rsnmpSOdNzQ` |
| `_ZN3JSC9CodeCache5writeERNS_2VME` | `cOnk31bagNI` |
| `_ZN3JSC9HandleSet12writeBarrierEPNS_7JSValueERKS1_` | `oSOlHR1+AKw` |
| `_ZN3JSC9Integrity14auditCellFullyERNS_2VMEPNS_6JSCellE` | `WC1M6Pb+YEg` |
| `_ZN3JSC9JSONParseEPNS_14JSGlobalObjectERKN3WTF6StringE` | `y0tHjQrzBJk` |
| `_ZN3JSC9Structure14sealTransitionERNS_2VMEPS0_` | `ICnB2IswiwU` |
| `_ZN3JSC9Structure16freezeTransitionERNS_2VMEPS0_` | `hEm8er7rP3U` |
| `_ZN3JSC9Structure25nonPropertyTransitionSlowERNS_2VMEPS0_NS_14TransitionKindE` | `QFQlVeu1W60` |
| `_ZN3JSC9Structure3pinERKN3WTF14AbstractLockerERNS_2VMEPNS_13PropertyTableE` | `kOWP4y3+WZE` |
| `_ZN3JSC9constructEPNS_14JSGlobalObjectENS_7JSValueERKNS_7ArgListEPKc` | `HAxW6DIwYBU` |
| `_ZN3JSC9constructEPNS_14JSGlobalObjectENS_7JSValueES2_RKNS_7ArgListEPKc` | `CPDt6gjsX9Y` |
| `_ZN3WTF10AtomString4initEv` | `nQvycLYTm7A` |
| `_ZN3WTF10StackTrace17captureStackTraceEii` | `fyBcK2uVX9E` |
| `_ZN3WTF10StackTrace8demangleEPv` | `9U1DoHp0Kcs` |
| `_ZN3WTF10StringImpl11reverseFindEPS0_j` | `GmJr4Guymvk` |
| `_ZN3WTF10StringImpl17createFromLiteralEPKcj` | `eiR-c3WYWAE` |
| `_ZN3WTF10StringImpl17createFromLiteralEPKc` | `mLHS0Q4GV-o` |
| `_ZN3WTF10StringImpl18simplifyWhiteSpaceEv` | `6kEVUeU2EL8` |
| `_ZN3WTF10StringImpl20create8BitIfPossibleEPKDs` | `2BtaIvBG7-Y` |
| `_ZN3WTF10StringImpl20createWithoutCopyingEPKDsj` | `XVb+0NN54gc` |
| `_ZN3WTF10StringImpl20createWithoutCopyingEPKhj` | `M0yz1LPGSaE` |
| `_ZN3WTF10StringImpl23defaultWritingDirectionEPb` | `vKObO5LG+x8` |
| `_ZN3WTF10StringImpl4findEPFbDsEj` | `KcAJ-JrL2+c` |
| `_ZN3WTF10StringImpl4findEPS0_j` | `noY4H2ZEXJw` |
| `_ZN3WTF10StringImpl4findEPS0_` | `wJvsHbLD7aI` |
| `_ZN3WTF10StringImpl6createEPKh` | `rUSjmJ0daUk` |
| `_ZN3WTF10StringImpl7replaceEDsPS0_` | `PoT9sdk4N18` |
| `_ZN3WTF10StringImpl7replaceEPS0_S1_` | `djdfuFWlVPI` |
| `_ZN3WTF10StringImpl7replaceEjjPS0_` | `MejItkj1J4c` |
| `_ZN3WTF10StringImpl8endsWithEPS0_` | `PTf-FJo+ApE` |
| `_ZN3WTF10StringImpl8endsWithERS0_` | `9dOBfs078pg` |
| `_ZN3WTF10nullStringEv` | `z7ey00+6y+Q` |
| `_ZN3WTF10protocolIsENS_10StringViewEPKc` | `A2BZJgwBmeI` |
| `_ZN3WTF11Persistence5CoderINS_10AtomStringEE6decodeERNS0_7DecoderE` | `4dtPhcCb76Y` |
| `_ZN3WTF11Persistence5CoderINS_6StringEE6decodeERNS0_7DecoderE` | `ZW5gWeh6BKQ` |
| `_ZN3WTF11Persistence5CoderINS_7CStringEE6decodeERNS0_7DecoderE` | `D93FsJ-Bib4` |
| `_ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE` | `3U2pDdbhjag` |
| `_ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_` | `jiOeDsjiDkY` |
| `_ZN3WTF11emptyStringEv` | `yDG0A0GSJX0` |
| `_ZN3WTF11setDataFileEOSt10unique_ptrINS_11PrintStreamESt14default_deleteIS1_EE` | `G4Jr+rsslPg` |
| `_ZN3WTF12randomNumberEv` | `f+79V4V+sLE` |
| `_ZN3WTF14AtomStringImpl3addEPKDs` | `CbzhOF12EMk` |
| `_ZN3WTF14AtomStringImpl3addEPKh` | `HykcoTLLx5o` |
| `_ZN3WTF14FileSystemImpl17openTemporaryFileERKNS_6StringERiS3_` | `kMydohzpE+Q` |
| `_ZN3WTF14FileSystemImpl24pathByAppendingComponentERKNS_6StringES3_` | `h3p-IDytuAk` |
| `_ZN3WTF15FilePrintStream4openEPKcS2_` | `g0vmm9VmCFY` |
| `_ZN3WTF16weakRandomUint32Ev` | `FPJ7KlJEPzo` |
| `_ZN3WTF17PrivateSymbolImpl16createNullSymbolEv` | `3cRqbsC4oQE` |
| `_ZN3WTF17TextBreakIteratorC1ENS_10StringViewENS0_4ModeERKNS_10AtomStringE` | `E0+yxDU5iW8` |
| `_ZN3WTF17TextBreakIteratorC2ENS_10StringViewENS0_4ModeERKNS_10AtomStringE` | `Up0zYyxuRLQ` |
| `_ZN3WTF19initializeThreadingEv` | `9zPBIupchgA` |
| `_ZN3WTF20ObjectIdentifierBase26generateIdentifierInternalEv` | `7Zihjr8aD+Y` |
| `_ZN3WTF20ObjectIdentifierBase36generateThreadSafeIdentifierInternalEv` | `l9kqixNsNq4` |
| `_ZN3WTF21LineBreakIteratorPool10sharedPoolEv` | `LVut7lXu51g` |
| `_ZN3WTF22defaultPortForProtocolENS_10StringViewE` | `8stlbyGspnE` |
| `_ZN3WTF23openLatin1UTextProviderEPNS_15UTextWithBufferEPKhjP10UErrorCode` | `RxRtEFonL5U` |
| `_ZN3WTF24acquireLineBreakIteratorENS_10StringViewERKNS_10AtomStringEPKDsjNS_21LineBreakIteratorModeE` | `OmR6AUTosbU` |
| `_ZN3WTF24releaseLineBreakIteratorEP14UBreakIterator` | `mqVRQv4SBS4` |
| `_ZN3WTF25createCanonicalUUIDStringEv` | `i1pLagnFZ5M` |
| `_ZN3WTF29cryptographicallyRandomNumberEv` | `wMu+dIbw7c0` |
| `_ZN3WTF31integerToSixCharacterHashStringEj` | `bhNTXZsyzkE` |
| `_ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE` | `ALtdQuwSy0A` |
| `_ZN3WTF4SHA19hexDigestERKSt5arrayIhLm20EE` | `pF6MaJY4tLM` |
| `_ZN3WTF6Logger9observersEv` | `aQHUuJt0YS8` |
| `_ZN3WTF6String23make16BitFrom8BitSourceEPKhm` | `1dq4iQIG88U` |
| `_ZN3WTF6String23make8BitFrom16BitSourceEPKDsm` | `I29SrxImX-A` |
| `_ZN3WTF6String6appendEDs` | `K51HUUV9AWA` |
| `_ZN3WTF6String6appendEPKDsj` | `iSHhSAr1jks` |
| `_ZN3WTF6String6appendEPKhj` | `1IIKnMD2xqM` |
| `_ZN3WTF6String6appendERKS0_` | `qEvRt3Wki8E` |
| `_ZN3WTF6String6appendEh` | `4AO3zU38Gd8` |
| `_ZN3WTF6String6insertERKS0_j` | `pFS2eHSDMLU` |
| `_ZN3WTF6String6removeEjj` | `6jZLakbLGTI` |
| `_ZN3WTF6String8truncateEj` | `ONNjULAZbZo` |
| `_ZN3WTF6StringC1ENS_12ASCIILiteralE` | `URX9kwFwkis` |
| `_ZN3WTF6StringC1EPKDs` | `HB6rCHl8fMs` |
| `_ZN3WTF6StringC1EPKh` | `xN7BBNHgLrs` |
| `_ZN3WTF6StringC2EPKDs` | `SE8wOZcQFuc` |
| `_ZN3WTF6StringC2EPKh` | `XXiJQqFMwl8` |
| `_ZN3WTF7CPUTime3getEv` | `jgHh6iq6MwI` |
| `_ZN3WTF7RunLoop6isMainEv` | `4znIAFaCTDE` |
| `_ZN3WTF7RunLoop7iterateEv` | `2TTHEYSAN-M` |
| `_ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b` | `HblNi06Y6UI` |
| `_ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb` | `UjTDE3AQCRg` |
| `_ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_` | `AJ7+0UjKEww` |
| `_ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb` | `PqZUVw-1dv0` |
| `_ZN3WTF8Internal9appendHexEPhjmjNS_17HexConversionModeE` | `J9Fo8pqmVZ4` |
| `_ZN3WTF8JSONImpl5Value12escapeStringERNS_13StringBuilderENS_10StringViewE` | `ewZNP1Jwrx8` |
| `_ZN3WTF8pageMaskEv` | `rIKEui9oRJg` |
| `_ZN7bmalloc11AllIsoHeapsC1ERKSt11scoped_lockIJNS_5MutexEEE` | `LZqSlU-3M0s` |
| `_ZN7bmalloc11AllIsoHeapsC2ERKSt11scoped_lockIJNS_5MutexEEE` | `VtFCouWs9cA` |
| `_ZN7bmalloc11EnvironmentC1ERKSt11scoped_lockIJNS_5MutexEEE` | `xdvsBbplHHc` |
| `_ZN7bmalloc11EnvironmentC2ERKSt11scoped_lockIJNS_5MutexEEE` | `SIqT0iWoeck` |
| `_ZN7bmalloc12IsoTLSLayoutC1ERKSt11scoped_lockIJNS_5MutexEEE` | `d8-j5KHO55s` |
| `_ZN7bmalloc12IsoTLSLayoutC2ERKSt11scoped_lockIJNS_5MutexEEE` | `ZcLLCGKGbjY` |
| `_ZN7bmalloc9ScavengerC1ERKSt11scoped_lockIJNS_5MutexEEE` | `qCgK7DkHGY4` |
| `_ZN7bmalloc9ScavengerC2ERKSt11scoped_lockIJNS_5MutexEEE` | `eCt77qfDBf8` |
| `_ZN9Inspector13AgentRegistry6appendESt10unique_ptrINS_18InspectorAgentBaseESt14default_deleteIS2_EE` | `73e3FnFmw6k` |
| `_ZN9Inspector14ConsoleMessageC1EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelEON3WTF6VectorINS5_12JSONLogValueELm0ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEPNS1_14JSGlobalObjectEm` | `JzLxHxosAio` |
| `_ZN9Inspector14ConsoleMessageC1EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelERKN3WTF6StringES8_jjPNS1_14JSGlobalObjectEm` | `J4h9Iq-fkQ4` |
| `_ZN9Inspector14ConsoleMessageC1EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelERKN3WTF6StringEm` | `bEOdEpgXaoo` |
| `_ZN9Inspector14ConsoleMessageC2EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelEON3WTF6VectorINS5_12JSONLogValueELm0ENS5_15CrashOnOverflowELm16ENS5_10FastMallocEEEPNS1_14JSGlobalObjectEm` | `QcHBTWQf1Fo` |
| `_ZN9Inspector14ConsoleMessageC2EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelERKN3WTF6StringES8_jjPNS1_14JSGlobalObjectEm` | `W7V0pqBz0b4` |
| `_ZN9Inspector14ConsoleMessageC2EN3JSC13MessageSourceENS1_11MessageTypeENS1_12MessageLevelERKN3WTF6StringEm` | `aQKBnWRO6xU` |
| `_ZN9Inspector14InjectedScriptC1EN10Deprecated12ScriptObjectEPNS_20InspectorEnvironmentE` | `32lzv0QiLbo` |
| `_ZN9Inspector14InjectedScriptC2EN10Deprecated12ScriptObjectEPNS_20InspectorEnvironmentE` | `kESljrwvBC4` |
| `_ZN9Inspector15RemoteInspector39sendAutomaticInspectionCandidateMessageEv` | `Atp9oheNRqg` |
| `_ZN9Inspector15ScriptCallStack6createERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `oRiKVtbDDgo` |
| `_ZN9Inspector15ScriptCallStackC1ERN3WTF6VectorINS_15ScriptCallFrameELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `kWQbgfGBRqQ` |
| `_ZN9Inspector18InjectedScriptBase8makeCallERN10Deprecated18ScriptFunctionCallE` | `eP-XvTjowrw` |
| `_ZN9Inspector18InjectedScriptBaseC1ERKN3WTF6StringEN10Deprecated12ScriptObjectEPNS_20InspectorEnvironmentE` | `LWvN9tXtNFM` |
| `_ZN9Inspector18InjectedScriptBaseC2ERKN3WTF6StringEN10Deprecated12ScriptObjectEPNS_20InspectorEnvironmentE` | `7H6fVQJIvw4` |
| `_ZN9Inspector18InspectorHeapAgent27nodeForHeapObjectIdentifierERN3WTF6StringEj` | `WqtJrLRSJ38` |
| `_ZN9Inspector21InjectedScriptManager20createInjectedScriptERKN3WTF6StringEPN3JSC14JSGlobalObjectEi` | `MXcF5LJjxR8` |
| `_ZN9Inspector21InjectedScriptManager20injectedScriptSourceEv` | `wqhRTaTyx9k` |
| `_ZN9Inspector21InspectorConsoleAgent19addMessageToConsoleESt10unique_ptrINS_14ConsoleMessageESt14default_deleteIS2_EE` | `n5j3zPO0VQI` |
| `_ZN9Inspector22InspectorDebuggerAgent15didRunMicrotaskEv` | `H98NR3TRB+g` |
| `_ZN9Inspector22InspectorDebuggerAgent16willRunMicrotaskEv` | `uuGLzGM+ngk` |
| `_ZN9Inspector22InspectorDebuggerAgent18didCancelAsyncCallENS0_13AsyncCallTypeEi` | `4i4lCtimDgg` |
| `_ZN9Inspector22InspectorDebuggerAgent19asyncCallIdentifierENS0_13AsyncCallTypeEi` | `KhymfIzaujY` |
| `_ZN9Inspector22InspectorDebuggerAgent20didDispatchAsyncCallEv` | `8+KmK2NNPww` |
| `_ZN9Inspector22InspectorDebuggerAgent20didScheduleAsyncCallEPN3JSC14JSGlobalObjectENS0_13AsyncCallTypeEib` | `mciNZtOn-2E` |
| `_ZN9Inspector22InspectorDebuggerAgent21willDispatchAsyncCallENS0_13AsyncCallTypeEi` | `Fa7tNy5W1JA` |
| `_ZN9Inspector22RemoteInspectionTarget25setRemoteDebuggingAllowedEb` | `q8XE9mDjoAk` |
| `_ZN9Inspector25ConsoleFrontendDispatcher15messagesClearedEv` | `MiEScFB0La8` |
| `_ZN9Inspector25ConsoleFrontendDispatcher25messageRepeatCountUpdatedEi` | `Q0C9YHqdzNc` |
| `_ZNK10Deprecated11ScriptValue10hasNoValueEv` | `ey5hOr8ltt4` |
| `_ZNK10Deprecated11ScriptValue7jsValueEv` | `R7XLWikJQtg` |
| `_ZNK3JSC11RegisterSet4dumpERN3WTF11PrintStreamE` | `WW0x+WVuT58` |
| `_ZNK3JSC14JSGlobalObject22remoteDebuggingEnabledEv` | `R7sNS8fgJQU` |
| `_ZNK3JSC17DebuggerCallFrame12functionNameEv` | `HWhvb-bEWy0` |
| `_ZNK3JSC17DebuggerCallFrame29deprecatedVMEntryGlobalObjectEv` | `s7PjdUkvY1s` |
| `_ZNK3JSC17DebuggerCallFrame4typeEv` | `3xusIXS+iL0` |
| `_ZNK3JSC4Yarr17RegularExpression5matchERKN3WTF6StringEiPi` | `cv8-Cxsrz1Y` |
| `_ZNK3JSC4Yarr17RegularExpression9searchRevERKN3WTF6StringE` | `ofaNlI4jEM0` |
| `_ZNK3JSC7JSValue14toThisSlowCaseEPNS_14JSGlobalObjectENS_8ECMAModeE` | `Hm5HSXOiUr0` |
| `_ZNK3JSC8JSObject43anyObjectInChainMayInterceptIndexedAccessesERNS_2VME` | `wSQv7alS9b4` |
| `_ZNK3JSC8Profiler8Database4toJSEPNS_14JSGlobalObjectE` | `Ipo19kq3hWA` |
| `_ZNK3JSC9ClassInfo35hasStaticSetterOrReadonlyPropertiesEv` | `iGxZRUg9NPM` |
| `_ZNK3WTF10StackTrace4dumpERNS_11PrintStreamEPKc` | `gfTs2aPeHoM` |
| `_ZNK3WTF10StringImpl10startsWithEPKS0_` | `2K6zyWD2WxI` |
| `_ZNK3WTF10StringImpl10startsWithERKS0_` | `cXUbCLbi6mw` |
| `_ZNK3WTF10StringImpl10tryGetUtf8ENS_14ConversionModeE` | `cz5x0BcGOZ8` |
| `_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j` | `Lk6zcChMMfw` |
| `_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j` | `8t7fiGWomPU` |
| `_ZNK3WTF10StringImpl18tryGetUtf8ForRangeEjjNS_14ConversionModeE` | `PcudeTUiShQ` |
| `_ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_j` | `kAsMu6O6yZ0` |
| `_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j` | `y8jha02Bsu0` |
| `_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_` | `lkxhdb4gfCk` |
| `_ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseEPKS0_` | `MvKToaKANpw` |
| `_ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseERKS0_` | `zKg5AOZL6V0` |
| `_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseEPKS0_` | `b355f1hsAGw` |
| `_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseERKS0_` | `CxxZ4m5WzGU` |
| `_ZNK3WTF10StringView10tryGetUtf8ENS_14ConversionModeE` | `T-uddfL1AmI` |
| `_ZNK3WTF10StringView16GraphemeClusters8IteratorneERKS2_` | `vFWzXVdtsvk` |
| `_ZNK3WTF10StringView8containsEPKc` | `CZHdjB7ArhA` |
| `_ZNK3WTF13StringBuilder10isAllASCIIEv` | `R5rWHiU8BF4` |
| `_ZNK3WTF3URL11isLocalFileEv` | `2awTIdIwODI` |
| `_ZNK3WTF3URL15protocolIsAboutEv` | `YeskHF2qK4I` |
| `_ZNK3WTF3URL38stringWithoutQueryOrFragmentIdentifierEv` | `Qlcy3AKepzU` |
| `_ZNK3WTF3URL4portEv` | `AxbrDa89hBs` |
| `_ZNK3WTF6String10tryGetUtf8ENS_14ConversionModeE` | `iQoJc7qieSk` |
| `_ZNK3WTF6String10tryGetUtf8Ev` | `87OOTOeB8pE` |
| `_ZNK3WTF6String15stripWhiteSpaceEv` | `gQ0v9iNBI44` |
| `_ZNK3WTF6String18simplifyWhiteSpaceEv` | `QYuJju6ucf8` |
| `_ZNK3WTF6String25splitAllowingEmptyEntriesERKS0_` | `A3YOM+5fr5A` |
| `_ZNK3WTF6String33stripLeadingAndTrailingCharactersEPFbDsE` | `E5qSFrM0WtE` |
| `_ZNK3WTF6String4utf8Ev` | `s+42ELWxjCI` |
| `_ZNK3WTF6String5splitERKS0_` | `i1zdtddvwdE` |
| `_ZNK3WTF6String9substringEjj` | `aTSGWfAX47k` |
| `_ZNK3WTF9MediaTimeneERKS0_` | `oJV+0NA8pss` |
| `_ZNK9Inspector15RemoteInspector18clientCapabilitiesEv` | `7HwcP1R63Eo` |
| `_ZNK9Inspector18InjectedScriptBase27callFunctionWithEvalEnabledERN10Deprecated18ScriptFunctionCallE` | `69Ng45oazLY` |
| `_ZNK9Inspector18InjectedScriptHost16savedResultAliasEv` | `1EYcTwBoE4w` |
| `_ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv` | `nYbNPPMMWik` |

### Additional unidentified export

- [NID `TYvNAgirhfM`](../../../core/libs/prx/libSceJsc/Export.cpp) is a throwing placeholder. Its name, signature and behavior are unverified. Matching module and library identities occur in two version-labelled images; those labels are not authenticated.

- [Five additional NIDs](../../../core/libs/prx/libSceJsc/Export.cpp) have callable entries in both version-labelled Jsc images. The later image also contains these NIDs in a distinct WebKit provider; behavior and equivalence are unverified, and future provider additions require loader analysis. Names, signatures and behavior are unknown; all five declarations throw.
