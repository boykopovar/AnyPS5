# libSceNKWebKit

## add Functional

- [Lifted wrappers](../../../core/libs/prx/libSceNKWebKit/src/LiftedWrappers.cpp) (libSceNKWebKit) - 93 exports whose body in the `libSceNKWebKit.sprx` with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` (the module the Constant-return exports entry reads as 11.00) has no branches and only calls other named exports, with loads, stores and arithmetic through its arguments. Each body was translated instruction by instruction into C++ that keeps every call, argument register, load and store in order. Callees that are still stubs throw, so these exports only work once their callees do. A low (null-page) address throws instead of faulting. The `LiftedWrappers` test replays recorded call logs and memory digests on Linux and checks null objects everywhere. Parameter and return types are register widths, not source prototypes.

## remove | `JSRunLoop` | `Olb6L5-YCBE` |

## remove | `JSSetOptions` | `vE61ounVJEI` |

## remove | `_ZN3JSC10JSFunctionC1ERNS_2VMEPNS_16NativeExecutableEPNS_14JSGlobalObjectEPNS_9StructureE` | `uNBcuOM-orU` |

## remove | `_ZN3JSC10JSFunctionC2ERNS_2VMEPNS_16NativeExecutableEPNS_14JSGlobalObjectEPNS_9StructureE` | `eVxwyigpkZI` |

## remove | `_ZN3JSC11VMInspector14dumpCellMemoryEPNS_6JSCellE` | `YsiVd2KRHzg` |

## remove | `_ZN3JSC12CachePayload16makeEmptyPayloadEv` | `kC-1WFDy3EU` |

## remove | `_ZN3JSC12CachePayload17makeMallocPayloadEON3WTF9MallocPtrIhNS1_10FastMallocEEEm` | `8O0T16ycgH8` |

## remove | `_ZN3JSC16CompleteSubspaceD0Ev` | `6S0zsQ1cbTs` |

## remove | `_ZN3JSC16SamplingProfiler18reportTopBytecodesEv` | `WULWTEqDWFE` |

## remove | `_ZN3JSC16SamplingProfiler18reportTopFunctionsEv` | `GphNWAcTBGE` |

## remove | `_ZN3JSC17DeferredWorkTimerD0Ev` | `DWWNTz7HFiA` |

## remove | `_ZN3JSC19HeapSnapshotBuilderD0Ev` | `b3xXLyo06sQ` |

## remove | `_ZN3JSC19InlineWatchpointSet11inflateSlowEv` | `hRVJOIGfek0` |

## remove | `_ZN3JSC21throwOutOfMemoryErrorEPNS_14JSGlobalObjectERNS_10ThrowScopeE` | `lf-7QriZ25o` |

## remove | `_ZN3JSC21throwOutOfMemoryErrorEPNS_14JSGlobalObjectERNS_10ThrowScopeERKN3WTF6StringE` | `Rw0L25eAWzQ` |

## remove | `_ZN3JSC22EdenGCActivityCallbackD0Ev` | `OM8XPyh1Dc4` |

## remove | `_ZN3JSC32JSDestructibleObjectHeapCellTypeD0Ev` | `YxdY94Fw670` |

## remove | `_ZN3JSC8Bindings10RootObjectD0Ev` | `Xk7pXSVYt5Q` |

## remove | `_ZN3JSC8DebuggerD0Ev` | `OLC0QBXKywQ` |

## remove | `_ZN3WTF18ExternalStringImpl6createEPKDsjONS_8FunctionIFvPS0_PvjEEE` | `8Elqwzjf6KQ` |

## remove | `_ZN3WTF18ExternalStringImpl6createEPKhjONS_8FunctionIFvPS0_PvjEEE` | `g6gKfjgJEGU` |

## remove | `_ZN7WebCore12ISOWebVTTCueD0Ev` | `AMXD4SYEIIg` |

## remove | `_ZN7WebCore12NamedNodeMap12setNamedItemERNS_4AttrE` | `OjJcnVmKx9M` |

## remove | `_ZN7WebCore12PrintContextD0Ev` | `QSWpsSZDHig` |

## remove | `_ZN7WebCore13MIMETypeCacheD0Ev` | `7VBlVFPhir4` |

## remove | `_ZN7WebCore14DocumentLoaderD0Ev` | `TxDI56HeuOQ` |

## remove | `_ZN7WebCore14JSVoidCallback6createEPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE` | `DnXatGyC6cw` |

## remove | `_ZN7WebCore14JSVoidCallbackD0Ev` | `nClH8DV3dsg` |

## remove | `_ZN7WebCore14ResourceLoader14cancelledErrorEv` | `mFx3dcK+S2c` |

## remove | `_ZN7WebCore14XMLHttpRequestD0Ev` | `vnLqlsltLJs` |

## remove | `_ZN7WebCore16DeviceMotionData6createEv` | `nRNa+ZeFvnY` |

## remove | `_ZN7WebCore16HTMLMediaElement19originsInMediaCacheERKN3WTF6StringE` | `M-aG2xrDafY` |

## remove | `_ZN7WebCore16JSStringCallback6createEPN3JSC8JSObjectEPNS_17JSDOMGlobalObjectE` | `mzFdkwKGq-I` |

## remove | `_ZN7WebCore16JSStringCallbackD0Ev` | `js0MFg3iZcU` |

## remove | `_ZN7WebCore16ScriptController30executeScriptIgnoringExceptionERKN3WTF6StringEb` | `dxphfFDV-4Y` |

## remove | `_ZN7WebCore17CredentialStorage24getFromPersistentStorageERKNS_15ProtectionSpaceE` | `7-2glF9P8AI` |

## remove | `_ZN7WebCore17HTMLAnchorElement4textEv` | `c6d0lcEF-j4` |

## remove | `_ZN7WebCore17HTMLCanvasElement9toDataURLERKN3WTF6StringE` | `uTL5h-iE3u0` |

## remove | `_ZN7WebCore17NowPlayingManagerD0Ev` | `N+JkZ-EmyFQ` |

## remove | `_ZN7WebCore18DOMWindowExtensionD0Ev` | `wTZgIXezihg` |

## remove | `_ZN7WebCore18TextureMapperLayerD0Ev` | `kONYtx9Wm-w` |

## remove | `_ZN7WebCore19MediaResourceLoaderD0Ev` | `VVR2dI1029A` |

## remove | `_ZN7WebCore22EmptyFrameLoaderClient20createDocumentLoaderERKNS_15ResourceRequestERKNS_14SubstituteDataE` | `HxybCxtAU6k` |

## remove | `_ZN7WebCore24CoordinatedGraphicsLayerD0Ev` | `m8JHnyIOhiY` |

## remove | `_ZN7WebCore4Page12setIsVisibleEb` | `zPDRirPat0g` |

## remove | `_ZN7WebCore6Editor26toggleOverwriteModeEnabledEv` | `l5qMgwcStrs` |

## remove | `_ZN7WebCore7Element9outerTextEv` | `DgOXxr1ZN8A` |

## remove | `_ZN7WebCore8Document11createRangeEv` | `xtgoMt+v-H0` |

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_11EventRegionE` | `eUaXBqzdC0Y` |

## remove | `_ZN9Inspector14InjectedScriptaSERKS0_` | `4cZX7bAkRdA` |

## remove | `_ZN9Inspector14InspectorAgentD0Ev` | `XEzAnNpXVaw` |

## remove | `_ZN9Inspector15RemoteInspector27clientCapabilitiesDidChangeEv` | `aanMqRmIwds` |

## remove | `_ZN9Inspector20InspectorTargetAgentD0Ev` | `WerviCFtn04` |

## remove | `_ZN9Inspector21InjectedScriptManagerD0Ev` | `HywoTZ3E+V4` |

## remove | `_ZN9Inspector21InspectorConsoleAgentD0Ev` | `Cbgna+rzoKg` |

## remove | `_ZN9Inspector28InspectorScriptProfilerAgentD0Ev` | `ikTNlkIaVhU` |

## remove | `_ZNK14OpaqueJSString6stringEv` | `7Xdw2-E1v4c` |

## remove | `_ZNK7WebCore11MediaPlayer15contentMIMETypeEv` | `aePIC2GrVwI` |

## remove | `_ZNK7WebCore11MediaPlayer17contentTypeCodecsEv` | `5dclua8Io5s` |

## remove | `_ZNK7WebCore12NamedNodeMap12getNamedItemERKN3WTF10AtomStringE` | `fgqfPqcWPM0` |

## remove | `_ZNK7WebCore12NamedNodeMap14getNamedItemNSERKN3WTF10AtomStringES4_` | `HopnL7CuhJk` |

## remove | `_ZNK7WebCore15JSDOMWindowBase22scriptExecutionContextEv` | `SHmLauSB1MU` |

## remove | `_ZNK7WebCore4Page9selectionEv` | `Ot1iqePgqhc` |

## remove | `_ZNK7WebCore6Editor7canEditEv` | `dpcMJMWSBkg` |

## remove | `_ZNK9Inspector20InspectorTargetAgent14connectionTypeEv` | `3wDKZRIqEA0` |

## remove | `_ZThn16_N9Inspector14InspectorAgentD0Ev` | `UQzEKNbOEzI` |

## remove | `_ZThn16_N9Inspector20InspectorTargetAgentD0Ev` | `tSxzKJq0HIw` |

## remove | `_ZThn16_N9Inspector21InspectorConsoleAgentD0Ev` | `wCnSbQWYyP4` |

## remove | `_ZThn16_N9Inspector28InspectorScriptProfilerAgentD0Ev` | `dbzPEmNY4C0` |

## remove | `_ZThn24_N7WebCore14DocumentLoaderD0Ev` | `056NZahTIcE` |

## remove | `_ZThn24_N7WebCore19MediaResourceLoaderD0Ev` | `NQao9f52Vtc` |

## remove | `_ZThn24_N9Inspector28InspectorScriptProfilerAgentD0Ev` | `ei92h4vDiPU` |

## remove | `_ZThn32_N7WebCore14DocumentLoaderD0Ev` | `v9lOD9R-HzU` |

## remove | `_ZThn32_N7WebCore14XMLHttpRequestD0Ev` | `w8S6R14D7AE` |
