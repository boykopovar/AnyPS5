# libSceNKWebKit

## add Functional

- [Empty-body exports](../../../core/libs/prx/libSceNKWebKit/src/NoOpExports.cpp) (libSceNKWebKit) - 112 more exports whose body in the `libSceNKWebKit.sprx` with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` (the module the Constant-return exports entry reads as 11.00) is a single `ret`. Each was checked against the pinned [PS5-WebKit-11.x](https://github.com/neonmodder123/PS5-WebKit-11.x/tree/80f77e94c63ee7202eec3d4b5b1a021dd89011ba) source where it still exists: header defaults (`FrameLoaderClient`, `CookieJar`, `RemoteInspectionTarget`, agent hooks), empty `EmptyFrameLoaderClient` and WK C API bodies, the PlayStation `notImplemented()` bodies of `Widget` and `Editor`, the libwpe `Pasteboard` bodies and `ASSERT`- or `ENABLE`-only bodies. Exports whose 11.x PlayStation source does real work (`OSAllocator::commit`/`decommit`, `Integrity::auditCellMinimallySlow`, `Editor::handleAlternativeTextUIResult`, `DocumentLoader::applyPoliciesToSettings`, `DocumentLoader::attachToFrame()`) are left out even though their module body is empty.
- [Constant-body exports](../../../core/libs/prx/libSceNKWebKit/src/ModuleConstantBodies.cpp) (libSceNKWebKit) - 38 exports whose body in the same module only produces a constant: `xor eax,eax; ret` or `mov al,1; ret` (declared here as `bool`, and `void*` for `ResourceResponse::cfURLResponse`), `mov qword [rdi],0; ret` for the two `CachedResourceHandleBase` and `PerGlobalObjectWrapperWorld` default constructors, and `mov rax,rdi` followed by zeroing 8, 16 or 32 bytes at `rdi` for functions returning a null `String`/`RefPtr`/`AtomString`, an empty `Vector` or a zero `WKRect` through the return slot. A null object or return-slot pointer throws, where the module would fault. The two `FrameLoaderClient` placement `operator new` exports, `mov rax,rsi; ret`, are in [ArgumentReturn.cpp](../../../core/libs/prx/libSceNKWebKit/src/ArgumentReturn.cpp).

## remove | `WKBundleNodeHandleGetHTMLInputElementAutoFillButtonBounds` | `hjt6ZGKuEfo` |

## remove | `WKBundleNodeHandleGetRenderRect` | `TBGBVDzPzh0` |

## remove | `WKBundlePageCopyRenderLayerTree` | `dCVd7TqyhYA` |

## remove | `WKBundlePageCopyRenderTree` | `8Wcn9B8z17M` |

## remove | `WKBundlePageSetDefersLoading` | `Rm6Y9lnzm3c` |

## remove | `WKCookieManagerDeleteAllCookiesModifiedAfterDate` | `iEypYwTA7F0` |

## remove | `WKCookieManagerDeleteCookiesForHostname` | `4gJ2MFHUsvY` |

## remove | `WKCookieManagerGetHTTPCookieAcceptPolicy` | `d9tCx7OTo00` |

## remove | `WKCookieManagerGetHostnamesWithCookies` | `FNCVZgUSgfw` |

## remove | `WKCookieManagerSetClient` | `q-Hwxr4yls0` |

## remove | `WKCookieManagerStartObservingCookieChanges` | `vyOiJyvWOik` |

## remove | `WKCookieManagerStopObservingCookieChanges` | `ebPeu+4eyj8` |

## remove | `_ZN3JSC15WeakHandleOwner8finalizeENS_6HandleINS_7UnknownEEEPv` | `OBwl-d2wkJ0` |

## remove | `_ZN3JSC16slowValidateCellEPNS_6JSCellE` | `JQ5PeKXGA2Y` |

## remove | `_ZN3JSC23AbstractSamplingCounter4dumpEv` | `NO8LGTKyCTk` |

## remove | `_ZN3WTF15AutomaticThread14threadDidStartEv` | `WgCZ0HyfQ3I` |

## remove | `_ZN3WTF15AutomaticThread16threadIsStoppingERKNS_14AbstractLockerE` | `s-PEdREq5oE` |

## remove | `_ZN3WTF25activateSignalHandlersForENS_6SignalE` | `Pyu7Zyt6XUE` |

## remove | `_ZN7WebCore10Pasteboard10readOriginEv` | `kBiv08+TCFs` |

## remove | `_ZN7WebCore10Pasteboard11writeMarkupERKN3WTF6StringE` | `ZgJhmu25z0w` |

## remove | `_ZN7WebCore10Pasteboard15writeCustomDataERKN3WTF6VectorINS_20PasteboardCustomDataELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `DDmxBXZkpHo` |

## remove | `_ZN7WebCore10Pasteboard20typesSafeForBindingsERKN3WTF6StringE` | `LyRrwIE5yMA` |

## remove | `_ZN7WebCore10Pasteboard22readStringInCustomDataERKN3WTF6StringE` | `gygzdpUOeyI` |

## remove | `_ZN7WebCore10Pasteboard33writeTrustworthyWebURLsPboardTypeERKNS_13PasteboardURLE` | `PEI-+9NQV6s` |

## remove | `_ZN7WebCore10Pasteboard5clearERKN3WTF6StringE` | `L+HtHRV6uMs` |

## remove | `_ZN7WebCore10Pasteboard5clearEv` | `SIyekYGjGW8` |

## remove | `_ZN7WebCore10Pasteboard5writeERKNS_15PasteboardImageE` | `cXWnhYkh4iA` |

## remove | `_ZN7WebCore10Pasteboard5writeERKNS_5ColorE` | `Y3ovKe32X2U` |

## remove | `_ZN7WebCore13GraphicsLayer16resumeAnimationsEv` | `QnocxaQBZOM` |

## remove | `_ZN7WebCore13GraphicsLayer17suspendAnimationsEN3WTF13MonotonicTimeE` | `JPMZdhX2qEA` |

## remove | `_ZN7WebCore15ActiveDOMObject4stopEv` | `7Uoplf70Yus` |

## remove | `_ZN7WebCore15ActiveDOMObject6resumeEv` | `nSoL-e4quJU` |

## remove | `_ZN7WebCore15ActiveDOMObject7suspendENS_19ReasonForSuspensionE` | `xUBz9+Iln64` |

## remove | `_ZN7WebCore15WindowEventLoop27breakToAllowRenderingUpdateEv` | `tX+X7SrADMY` |

## remove | `_ZN7WebCore16HTMLMediaElement32prepareForVideoFullscreenStandbyEv` | `5DLa+e3Daho` |

## remove | `_ZN7WebCore20LowPowerModeNotifierC1EON3WTF8FunctionIFvbEEE` | `iraaucgzhMA` |

## remove | `_ZN7WebCore20LowPowerModeNotifierC2EON3WTF8FunctionIFvbEEE` | `-N8JU4FqoZg` |

## remove | `_ZN7WebCore21NetworkStorageSession10getCookiesERKN3WTF3URLE` | `L2dkX9ZmiqM` |

## remove | `_ZN7WebCore21NetworkStorageSession17domCookiesForHostERKN3WTF6StringE` | `6ajQEiaMYXw` |

## remove | `_ZN7WebCore22EmptyFrameLoaderClient29savePlatformDataToCachedFrameEPNS_11CachedFrameE` | `DF2bDNLQbUM` |

## remove | `_ZN7WebCore22EmptyFrameLoaderClient33dispatchWillPerformClientRedirectERKN3WTF3URLEdNS1_8WallTimeENS_19LockBackForwardListE` | `6q1eUV0OZoE` |

## remove | `_ZN7WebCore24CachedResourceHandleBaseC1Ev` | `2pkq+agYpkc` |

## remove | `_ZN7WebCore24CachedResourceHandleBaseC2Ev` | `270EeLhlxKE` |

## remove | `_ZN7WebCore24FrameDestructionObserver14willDetachPageEv` | `lbarclZ21IU` |

## remove | `_ZN7WebCore28InspectorFrontendClientLocal8Settings11setPropertyERKN3WTF6StringES5_` | `I9uZ6uF29YY` |

## remove | `_ZN7WebCore28InspectorFrontendClientLocal8Settings14deletePropertyERKN3WTF6StringE` | `QfBKuqPXKZo` |

## remove | `_ZN7WebCore4Icon18createIconForFilesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `KJgvss90XpU` |

## remove | `_ZN7WebCore4Node17stopIgnoringLeaksEv` | `mx3h6KLAu9Y` |

## remove | `_ZN7WebCore4Node18startIgnoringLeaksEv` | `4c1ohhNTURs` |

## remove | `_ZN7WebCore6Editor22writeImageToPasteboardERNS_10PasteboardERNS_7ElementERKN3WTF3URLERKNS5_6StringE` | `N8X38WX+rYU` |

## remove | `_ZN7WebCore6Editor26writeSelectionToPasteboardERNS_10PasteboardE` | `Azw7PDTow7I` |

## remove | `_ZN7WebCore6Widget4hideEv` | `Cs4aegJdHFU` |

## remove | `_ZN7WebCore6Widget4showEv` | `ee2ww7pIttw` |

## remove | `_ZN7WebCore6Widget8setFocusEb` | `t1svU-fltEM` |

## remove | `_ZN7WebCore9CookieJar10clearCacheEv` | `Fo--ToR1fsg` |

## remove | `_ZN7WebCore9CookieJar17clearCacheForHostERKN3WTF6StringE` | `aTamWRbZ1Y8` |

## remove | `_ZN9Inspector14InspectorAgent27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE` | `mhviITFdt+M` |

## remove | `_ZN9Inspector18InspectorHeapAgent27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE` | `m8QzC6ghMnE` |

## remove | `_ZN9Inspector19InspectorAuditAgent11muteConsoleEv` | `i3jcNNLDrjA` |

## remove | `_ZN9Inspector19InspectorAuditAgent13unmuteConsoleEv` | `vGOMeCbW94Q` |

## remove | `_ZN9Inspector19InspectorAuditAgent27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE` | `U+h+LTp3UDg` |

## remove | `_ZN9Inspector19InspectorAuditAgent29willDestroyFrontendAndBackendENS_16DisconnectReasonE` | `ezuirSOFGDo` |

## remove | `_ZN9Inspector21InjectedScriptManager23didCreateInjectedScriptERKNS_14InjectedScriptE` | `3PnKtGBM91A` |

## remove | `_ZN9Inspector21InjectedScriptManager7connectEv` | `7fXJ1BHjMmY` |

## remove | `_ZN9Inspector21InspectorConsoleAgent27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE` | `Vo3OWY+Pr9Q` |

## remove | `_ZN9Inspector21InspectorRuntimeAgent27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE` | `e9w1QH3vMDw` |

## remove | `_ZN9Inspector22InspectorDebuggerAgent27didClearAsyncStackTraceDataEv` | `yLNkraMvdcg` |

## remove | `_ZN9Inspector22RemoteInspectionTarget13setIndicatingEb` | `OVjZSfT7QFM` |

## remove | `_ZN9Inspector27PerGlobalObjectWrapperWorldC1Ev` | `bgd3ki6qpxU` |

## remove | `_ZN9Inspector27PerGlobalObjectWrapperWorldC2Ev` | `8rjATNfmywY` |

## remove | `_ZN9Inspector28InspectorScriptProfilerAgent27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE` | `SK-rxk8oEIU` |

## remove | `_ZNK7WebCore14LoaderStrategy12ongoingLoadsEv` | `m8rqXLGqxH4` |

## remove | `_ZNK7WebCore15ActiveDOMObject30assertSuspendIfNeededWasCalledEv` | `wRA9-Ih1MkU` |

## remove | `_ZNK7WebCore18PlatformPasteboard29typesSafeForDOMToReadAndWriteERKN3WTF6StringE` | `LEoZMfSH6Ls` |

## remove | `_ZNK7WebCore22EmptyFrameLoaderClient17overrideMediaTypeEv` | `bh4bKfeivgQ` |

## remove | `_ZNK9Inspector15RemoteInspector6Client11browserNameEv` | `36hv5tEW5cc` |

## remove | `_ZNK9Inspector15RemoteInspector6Client14browserVersionEv` | `NcuyC-lqbs4` |

## remove | `_ZNK9Inspector22RemoteInspectionTarget3urlEv` | `r20Y87D+ibY` |

## remove | `_ZNK9Inspector22RemoteInspectionTarget4nameEv` | `hep5VUibI2A` |
