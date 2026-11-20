# libSceNKWebKit

## add Functional

- [Lifted branchy wrappers](../../../core/libs/prx/libSceNKWebKit/src/LiftedBranchyWrappers.cpp) (libSceNKWebKit) - 53 exports whose body in the `libSceNKWebKit.sprx` with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` (the module the Constant-return exports entry reads as 11.00) has forward branches only (no loops, globals or stack locals) and calls other named exports, with loads, stores and arithmetic through its arguments. Each body was translated instruction by instruction into C++ that keeps every branch, call, argument register, load and store in order. Callees that are still stubs throw, so these exports only work once their callees do. A low (null-page) address throws instead of faulting. The `LiftedBranchyWrappers` test replays recorded call logs and memory digests on Linux and checks null objects everywhere. Parameter and return types are register widths, not source prototypes.

## remove | `WKAccessibilityRootObject` | `LGfu3uVGjDY` |

## remove | `_ZN3JSC12StackVisitor5Frame8sourceIDEv` | `T0R5EFfwaTg` |

## remove | `_ZN3JSC17DebuggerCallFrame20sourceIDForCallFrameEPNS_9CallFrameE` | `XBGl9HsG7rs` |

## remove | `_ZN3JSC9Structure16isValidPrototypeENS_7JSValueE` | `mTUZfpGBKD4` |

## remove | `_ZN3WTF10StringView16GraphemeClusters8IteratorD1Ev` | `SsbDxo-RZfo` |

## remove | `_ZN3WTF10StringView16GraphemeClusters8IteratorD2Ev` | `C+IZRnhqqG8` |

## remove | `_ZN7WebCore11MemoryCache14evictResourcesEv` | `snVIK-VDeCo` |

## remove | `_ZN7WebCore12EventHandler30setCapturingMouseEventsElementEPNS_7ElementE` | `Ho7945yolEI` |

## remove | `_ZN7WebCore12JSTimeRangesD1Ev` | `gLU-NsaNP3k` |

## remove | `_ZN7WebCore12JSTimeRangesD2Ev` | `P2S3--pUqtw` |

## remove | `_ZN7WebCore12JSTimeRangesaSERKS0_` | `z5ufrutjJsA` |

## remove | `_ZN7WebCore13JSDOMRectListD1Ev` | `36vtOQuRF8o` |

## remove | `_ZN7WebCore13JSDOMRectListD2Ev` | `JI4ly8BJLdc` |

## remove | `_ZN7WebCore13JSDOMRectListaSERKS0_` | `hhF-BMjB2+0` |

## remove | `_ZN7WebCore13displayStringERKN3WTF6StringEPKNS_4NodeE` | `lkcoWfBi-Eo` |

## remove | `_ZN7WebCore23ApplicationCacheStorageC1ERKN3WTF6StringES4_` | `IsPcsC1N-6I` |

## remove | `_ZN7WebCore23ApplicationCacheStorageC2ERKN3WTF6StringES4_` | `24QWHtxr2EM` |

## remove | `_ZN7WebCore23MediaQueryParserContextC1ERKNS_8DocumentE` | `vtJllvqhN2I` |

## remove | `_ZN7WebCore23MediaQueryParserContextC2ERKNS_8DocumentE` | `7UkDkleTd3I` |

## remove | `_ZN7WebCore30makeRangeSelectingNodeContentsERNS_4NodeE` | `veUz0LV2k1w` |

## remove | `_ZN7WebCore4Node11appendChildERS0_` | `nKaqdLF-yoo` |

## remove | `_ZN7WebCore4Node11removeChildERS0_` | `pgnOeOF2Nkc` |

## remove | `_ZN7WebCore4Node6removeEv` | `OVD8IJm1crQ` |

## remove | `_ZN7WebCore8JSPath2DD1Ev` | `Atil+QRPGs8` |

## remove | `_ZN7WebCore8JSPath2DD2Ev` | `zIttPL4lCv0` |

## remove | `_ZN7WebCore8JSPath2DaSERKS0_` | `7rfErhFzrTM` |

## remove | `_ZN7bmalloc13IsoSharedPage9tryCreateEv` | `A4P8aG8cGz0` |

## remove | `_ZN9Inspector22InspectorDebuggerAgent19clearExceptionValueEv` | `yeRcOaMeUE0` |

## remove | `_ZN9Inspector31NetworkBackendDispatcherHandler20LoadResourceCallbackD1Ev` | `6nkw2jvWJUY` |

## remove | `_ZN9Inspector32DatabaseBackendDispatcherHandler18ExecuteSQLCallbackD1Ev` | `vlJgDNWp6EM` |

## remove | `_ZN9Inspector32DatabaseBackendDispatcherHandler18ExecuteSQLCallbackD2Ev` | `UpfMQXD5L4M` |

## remove | `_ZNK7WebCore12NamedNodeMap6lengthEv` | `LeU1W3saPaM` |

## remove | `_ZNK7WebCore13HitTestResult11textContentEv` | `XW73Ink4DfA` |

## remove | `_ZNK7WebCore13HitTestResult15absoluteLinkURLEv` | `0cEED80Q7cU` |

## remove | `_ZNK7WebCore14DocumentLoader21archiveResourceForURLERKN3WTF3URLE` | `wchZ9bYTOdM` |

## remove | `_ZNK7WebCore14FrameSelection18isFocusedAndActiveEv` | `8Rc62+-s1+M` |

## remove | `_ZNK7WebCore14FrameSelection36rootEditableElementOrDocumentElementEv` | `knrcu8CFvhw` |

## remove | `_ZNK7WebCore16DOMGuardedObject11isSuspendedEv` | `J0v4IwoeBvc` |

## remove | `_ZNK7WebCore27ScriptedAnimationController17throttlingReasonsEv` | `V0EEd3DZsj8` |

## remove | `_ZNK7WebCore4Page27pageExtendedBackgroundColorEv` | `EJbXsBT7uts` |

## remove | `_ZNK7WebCore6Editor6clientEv` | `m8h5wdC8xOw` |

## remove | `_ZNK7WebCore6Editor9canDeleteEv` | `Rsrf1HN3218` |

## remove | `_ZNK7WebCore8Document6domainEv` | `xUDTB4S7g20` |

## remove | `_ZNK9Inspector15ScriptCallFrame7isEqualERKS0_` | `osFy-sM72sI` |
