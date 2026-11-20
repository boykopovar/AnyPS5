# libSceNKWebKit

## add Functional

- [Lifted virtual thunks](../../../core/libs/prx/libSceNKWebKit/src/LiftedVirtualThunks.cpp) (libSceNKWebKit) - 115 exports whose body in the `libSceNKWebKit.sprx` with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` (the module the Constant-return exports entry reads as 11.00) reads a function pointer from an object's vtable, or takes it from an argument, and jumps to it, with a null check, loads, stores or a reference-count update around it. Each body was translated instruction by instruction into C++ that keeps every load, store, branch and the indirect call in order, forwarding all six integer argument registers. The target is unknown, so the result is whatever it leaves in the result register; exports whose real return type is wider than that or not an integer are left out. The `LiftedVirtualThunks` test replays recorded call logs and memory digests against a fake recording vtable at a fixed address (x86-64).

## remove | `WKBundlePageOverlaySetAccessibilityClient` | `JBugz1LKFAA` |

## remove | `WKPageRunBeforeUnloadConfirmPanelResultListenerCall` | `T-GOS0yg738` |

## remove | `WKPageRunJavaScriptAlertResultListenerCall` | `6LmTCyxtXgo` |

## remove | `WKPageRunJavaScriptConfirmResultListenerCall` | `8tUBw7skPZ0` |

## remove | `WKWebsiteDataStoreClearAppBoundSession` | `YujMtLJPOok` |

## remove | `WKWebsiteDataStoreRemoveAllServiceWorkerRegistrations` | `scGRzF-d0Cs` |

## remove | `_ZN3JSC8Bindings13RuntimeObjectD1Ev` | `mryKx6akQCM` |

## remove | `_ZN3JSC8Bindings13RuntimeObjectD2Ev` | `xX9ex6bQiBI` |

## remove | `_ZN3JSC8Debugger23setBreakpointsActivatedEb` | `yKKINymjSZA` |

## remove | `_ZN3WTF21MemoryPressureHandler26endSimulatedMemoryPressureEv` | `6+8N+ozBlqQ` |

## remove | `_ZN3WTF22CrossThreadTaskHandler21setCompletionCallbackEONS_8FunctionIFvvEEE` | `3QQmCLX9eCA` |

## remove | `_ZN7WebCore10JSDocumentC1ERKS0_` | `CvrQw-QIoxQ` |

## remove | `_ZN7WebCore10JSDocumentC2ERKS0_` | `dZ7FLitHdjs` |

## remove | `_ZN7WebCore10JSDocumentD1Ev` | `46bLh4C9Dio` |

## remove | `_ZN7WebCore10JSDocumentD2Ev` | `xVcVD7jgI9k` |

## remove | `_ZN7WebCore10ScrollView17setUseFixedLayoutEb` | `yOJC0BpCABE` |

## remove | `_ZN7WebCore10ScrollView18setFixedLayoutSizeERKNS_7IntSizeE` | `URNifsiKAk0` |

## remove | `_ZN7WebCore11MediaPlayer11muteChangedEb` | `VwqoXNdHHNA` |

## remove | `_ZN7WebCore11MediaPlayer13volumeChangedEd` | `xCeTs4XzS8k` |

## remove | `_ZN7WebCore11MediaPlayer17setPreservesPitchEb` | `3a+wuXU3LT0` |

## remove | `_ZN7WebCore11MediaPlayer19prepareForRenderingEv` | `6MuMVeHVIA0` |

## remove | `_ZN7WebCore11MediaPlayer21endSimulatedHDCPErrorEv` | `4UZZcXmQt9Y` |

## remove | `_ZN7WebCore11MediaPlayer21setShouldDisableSleepEb` | `DXdvy5xNlE4` |

## remove | `_ZN7WebCore11MediaPlayer22setPrivateBrowsingModeEb` | `vpD3oDcNC7s` |

## remove | `_ZN7WebCore11MediaPlayer23beginSimulatedHDCPErrorEv` | `Td8wQ0FqTqg` |

## remove | `_ZN7WebCore11MediaPlayer28setPreferredDynamicRangeModeENS_16DynamicRangeModeE` | `On4LzFMH93c` |

## remove | `_ZN7WebCore11MediaPlayer8setMutedEb` | `L+MKiauGHXU` |

## remove | `_ZN7WebCore11MediaPlayer9setVolumeEd` | `woGXdge3elg` |

## remove | `_ZN7WebCore12JSAudioTrackD1Ev` | `z5gEC3cj9vo` |

## remove | `_ZN7WebCore12JSAudioTrackD2Ev` | `74E3+ZonU5Q` |

## remove | `_ZN7WebCore13JSHTMLElementC1ERKS0_` | `192-IV4CFa8` |

## remove | `_ZN7WebCore13JSHTMLElementC2ERKS0_` | `TuAOjJM7csM` |

## remove | `_ZN7WebCore13JSHTMLElementD1Ev` | `irgXGd58omE` |

## remove | `_ZN7WebCore13JSHTMLElementD2Ev` | `v6CSwwcnMO8` |

## remove | `_ZN7WebCore14JSWebAnimationC1ERKS0_` | `SWejTyuxZXQ` |

## remove | `_ZN7WebCore14JSWebAnimationC2ERKS0_` | `6x1JXGn6Yqk` |

## remove | `_ZN7WebCore14JSWebAnimationD1Ev` | `L3Nq23JAlvU` |

## remove | `_ZN7WebCore14JSWebAnimationD2Ev` | `MmSyyiaXiho` |

## remove | `_ZN7WebCore14ResourceHandle6cancelEv` | `5H5ytGoOj0A` |

## remove | `_ZN7WebCore14ScrollableArea21scrollbarStyleChangedENS_14ScrollbarStyleEb` | `xUt7yXvFz3M` |

## remove | `_ZN7WebCore15JSFetchResponseD1Ev` | `OKaYwlwKZG4` |

## remove | `_ZN7WebCore15JSFetchResponseD2Ev` | `SQW8red2Ny4` |

## remove | `_ZN7WebCore15JSSVGSVGElementC1ERKS0_` | `0Bsyz501NWA` |

## remove | `_ZN7WebCore15JSSVGSVGElementC2ERKS0_` | `1AbLXkyjTJo` |

## remove | `_ZN7WebCore15JSSVGSVGElementD1Ev` | `HsBiFJJIQ5Q` |

## remove | `_ZN7WebCore15JSSVGSVGElementD2Ev` | `JZ7r1NnPrt4` |

## remove | `_ZN7WebCore15XPathExpressionD1Ev` | `aKaMcVLBKfM` |

## remove | `_ZN7WebCore15XPathExpressionD2Ev` | `RhG8+YJU75Q` |

## remove | `_ZN7WebCore16HTMLMediaElement30setWebkitClosedCaptionsVisibleEb` | `xYmwUKajQbI` |

## remove | `_ZN7WebCore16JSXMLHttpRequestD1Ev` | `KIIYva3fxFM` |

## remove | `_ZN7WebCore16JSXMLHttpRequestD2Ev` | `L2BqedAiKTk` |

## remove | `_ZN7WebCore17JSHTMLLinkElementC1ERKS0_` | `tft0vpf8GIs` |

## remove | `_ZN7WebCore17JSHTMLLinkElementC2ERKS0_` | `bd-3fcfIMCk` |

## remove | `_ZN7WebCore17JSHTMLLinkElementD1Ev` | `Pph0Anbgy3w` |

## remove | `_ZN7WebCore17JSHTMLLinkElementD2Ev` | `D32g7jPlu4E` |

## remove | `_ZN7WebCore18JSHTMLImageElementC1ERKS0_` | `Obg5jzvJ3Po` |

## remove | `_ZN7WebCore18JSHTMLImageElementC2ERKS0_` | `l-NrXOC-Z4o` |

## remove | `_ZN7WebCore18JSHTMLImageElementD1Ev` | `MSRPfRgj8d4` |

## remove | `_ZN7WebCore18JSHTMLImageElementD2Ev` | `P-X-pyb+DqQ` |

## remove | `_ZN7WebCore18JSHTMLInputElementD1Ev` | `3Qhb7xN3SlY` |

## remove | `_ZN7WebCore18JSHTMLInputElementD2Ev` | `ruaWrFPIjFw` |

## remove | `_ZN7WebCore18JSHTMLMediaElementC1ERKS0_` | `TXSNtR-LjE4` |

## remove | `_ZN7WebCore18JSHTMLMediaElementC2ERKS0_` | `Ve-npIABD0g` |

## remove | `_ZN7WebCore18JSHTMLMediaElementD1Ev` | `Irg3UX8TFzM` |

## remove | `_ZN7WebCore18JSHTMLMediaElementD2Ev` | `t9T3rqRsy40` |

## remove | `_ZN7WebCore18JSHTMLVideoElementC1ERKS0_` | `XAF7SiZDFto` |

## remove | `_ZN7WebCore18JSHTMLVideoElementC2ERKS0_` | `TGyF9uiUGQY` |

## remove | `_ZN7WebCore18JSHTMLVideoElementD1Ev` | `lA9AH3d2f18` |

## remove | `_ZN7WebCore18JSHTMLVideoElementD2Ev` | `7RoTnYEHEFw` |

## remove | `_ZN7WebCore19JSAnimationTimelineD1Ev` | `xkLfem4ShVY` |

## remove | `_ZN7WebCore19JSAnimationTimelineD2Ev` | `FxH74ylow+Q` |

## remove | `_ZN7WebCore19JSHTMLSelectElementC1ERKS0_` | `33EJeb8F35g` |

## remove | `_ZN7WebCore19JSHTMLSelectElementC2ERKS0_` | `1GzMA6ls7io` |

## remove | `_ZN7WebCore19JSHTMLSelectElementD1Ev` | `GWy1Z5IV65E` |

## remove | `_ZN7WebCore19JSHTMLSelectElementD2Ev` | `6ljlgovZafE` |

## remove | `_ZN7WebCore21JSCSSStyleDeclarationC1ERKS0_` | `ENCQTNg5e+k` |

## remove | `_ZN7WebCore21JSCSSStyleDeclarationC2ERKS0_` | `NNtRrAFE6+U` |

## remove | `_ZN7WebCore21JSCSSStyleDeclarationD1Ev` | `jZGAl9Y-FkM` |

## remove | `_ZN7WebCore21JSCSSStyleDeclarationD2Ev` | `kf57RhzuP0g` |

## remove | `_ZN7WebCore21JSTextTrackCueGenericD1Ev` | `1mFJ6hh-6yk` |

## remove | `_ZN7WebCore21JSTextTrackCueGenericD2Ev` | `h5X9wHxHGOY` |

## remove | `_ZN7WebCore28InspectorFrontendClientLocal15changeSheetRectERKNS_9FloatRectE` | `9AXx+6CVOxs` |

## remove | `_ZN7WebCore4Page12whenUnnestedEON3WTF8FunctionIFvvEEE` | `fQjii2zz1Vk` |

## remove | `_ZN7WebCore4Page29applicationDidEnterBackgroundEv` | `EqcvUDNk4kE` |

## remove | `_ZN7WebCore4Page30applicationWillEnterForegroundEv` | `2aa1x2FcJT0` |

## remove | `_ZN7WebCore6JSFileD1Ev` | `5YqK1eWNQnM` |

## remove | `_ZN7WebCore6JSFileD2Ev` | `keOaOFLkcQQ` |

## remove | `_ZN7WebCore6JSNodeC1ERKS0_` | `tJc2S26es-Q` |

## remove | `_ZN7WebCore6JSNodeC2ERKS0_` | `5wDkfxnmbfI` |

## remove | `_ZN7WebCore6JSNodeD1Ev` | `t4PRYawDi44` |

## remove | `_ZN7WebCore6JSNodeD2Ev` | `JQrajDnmJR0` |

## remove | `_ZN7WebCore6Widget16removeFromParentEv` | `mvVhzhPPtag` |

## remove | `_ZN7WebCore7JSRangeD1Ev` | `RgMwHBEqK0o` |

## remove | `_ZN7WebCore7JSRangeD2Ev` | `hCyY74swF4U` |

## remove | `_ZN7WebCore9JSElementC1ERKS0_` | `wCfhaj6ayFw` |

## remove | `_ZN7WebCore9JSElementC2ERKS0_` | `dW0RTlUeeMI` |

## remove | `_ZN7WebCore9JSElementD1Ev` | `uuTo7JlfhX4` |

## remove | `_ZN7WebCore9Scrollbar13setProportionEii` | `gdVoksa+OAc` |

## remove | `_ZN9Inspector15InspectorTarget17setResumeCallbackEON3WTF8FunctionIFvvEEE` | `mpk5RNj6HKg` |

## remove | `_ZN9Inspector21InjectedScriptManager10disconnectEv` | `uK-XPIiI+jQ` |

## remove | `_ZNK7WebCore15ActiveDOMObject18hasPendingActivityEv` | `H-gEkxHy3s8` |

## remove | `_ZNK7WebCore16HTMLMediaElement23webkitHasClosedCaptionsEv` | `2XFOG9cQuV4` |

## remove | `_ZNK7WebCore6Editor7Command11isSupportedEv` | `qB2iptxCJXY` |

## remove | `_ZNK9Inspector18InjectedScriptBase31hasAccessToInspectedScriptStateEv` | `gNQcjhVwM+U` |

## remove | `_ZNK9Inspector21InspectorConsoleAgent22developerExtrasEnabledEv` | `7zFt1IsCMf4` |

## remove | `_ZNK9Inspector22RemoteInspectionTarget20remoteControlAllowedEv` | `QGapaf1-Pp0` |
