# libSceNKWebKit

## add Functional

- [Lifted leaf functions](../../../core/libs/prx/libSceNKWebKit/src/LiftedLeafFunctions.cpp) (libSceNKWebKit) - 192 exports whose body in the `libSceNKWebKit.sprx` with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` (the module the Constant-return exports entry reads as 11.00) is a leaf function (no calls or global data), translated instruction by instruction into C++ that keeps every load and store with its width and order. Parameter and return types are the register widths the bodies use, not the source prototypes. A null-page address throws where the module would fault. Checked offline against the module's own code on randomized memory; the `LiftedLeafFunctions` test replays recorded digests and contains no module code.

## remove | `_ZN3JSC11FuzzerAgent13getPredictionEPNS_9CodeBlockERKNS_10CodeOriginEm` | `bPu4g9cnHTE` |

## remove | `_ZN3JSC11MarkedBlock13areMarksStaleEv` | `k4PNvKkDUxg` |

## remove | `_ZN3JSC11MarkedBlock6Handle13areMarksStaleEv` | `fmDMZjA+Eg4` |

## remove | `_ZN3JSC13RuntimeMethod15createPrototypeERNS_2VMERNS_14JSGlobalObjectE` | `38B5Qc5Doj0` |

## remove | `_ZN3JSC13RuntimeMethod15subspaceForImplERNS_2VME` | `E51l7UtV1-A` |


## remove | `_ZN3JSC20failNextNewCodeBlockEPNS_14JSGlobalObjectE` | `w00D0ddSQLA` |

## remove | `_ZN3JSC22EdenGCActivityCallback12lastGCLengthERNS_4HeapE` | `ROmH4NlodXE` |

## remove | `_ZN3JSC22FullGCActivityCallback12lastGCLengthERNS_4HeapE` | `cRMHBi70QRE` |

## remove | `_ZN3JSC6JSCell9getObjectEv` | `MNu0IMCZZ6A` |

## remove | `_ZN3JSC8Bindings13RuntimeObject15createPrototypeERNS_2VMERNS_14JSGlobalObjectE` | `HIWq9DRwkyc` |

## remove | `_ZN3JSC8Bindings13RuntimeObject15subspaceForImplERNS_2VME` | `y-xe7dpaNBY` |

## remove | `_ZN3JSC8Debugger10isAttachedEPNS_14JSGlobalObjectE` | `oAF2eBhe0Zw` |

## remove | `_ZN3WTF10StringView16GraphemeClusters8IteratorC1EOS2_` | `WKOxC3orqKk` |

## remove | `_ZN3WTF10StringView16GraphemeClusters8IteratorC2EOS2_` | `Tuygbxm9F-A` |

## remove | `_ZN3WTF15AutomaticThread9isWaitingERKNS_14AbstractLockerE` | `qGldV9JFaJ8` |

## remove | `_ZN3WTF24AutomaticThreadConditionC1Ev` | `eyBB0eDGq5Y` |

## remove | `_ZN3WTF24AutomaticThreadConditionC2Ev` | `AiOIdr8qlv8` |

## remove | `_ZN7WebCore10JSLocationC1ERKS0_` | `fczC6fqKCAo` |

## remove | `_ZN7WebCore10JSLocationC2ERKS0_` | `9EcTld4VfWE` |

## remove | `_ZN7WebCore11DisplayList20FillRectWithGradientC1ERKNS_9FloatRectERNS_8GradientE` | `hgbEy0LBEgc` |

## remove | `_ZN7WebCore11DisplayList20FillRectWithGradientC2ERKNS_9FloatRectERNS_8GradientE` | `J+uDQ2Xphdo` |

## remove | `_ZN7WebCore11JSDOMMatrixC1ERKS0_` | `AY5HtCxTNwQ` |

## remove | `_ZN7WebCore11JSDOMMatrixC2ERKS0_` | `I9O9l17oQIo` |

## remove | `_ZN7WebCore11JSImageDataC1ERKS0_` | `r3Y3UiOISoo` |

## remove | `_ZN7WebCore11JSImageDataC2ERKS0_` | `N1Fa3I-h3+s` |

## remove | `_ZN7WebCore11SimpleRangeC1EONS_13BoundaryPointES2_` | `Yh3Q1rMIGXo` |

## remove | `_ZN7WebCore11SimpleRangeC1ERKNS_13BoundaryPointES3_` | `qrmirrWJQR8` |

## remove | `_ZN7WebCore11SimpleRangeC2EONS_13BoundaryPointES2_` | `4vKq7BQr9Og` |

## remove | `_ZN7WebCore11SimpleRangeC2ERKNS_13BoundaryPointES3_` | `PtI6Z08-raU` |

## remove | `_ZN7WebCore12JSTimeRangesC1ERKS0_` | `5VDOSnuKQcI` |

## remove | `_ZN7WebCore12JSTimeRangesC2ERKS0_` | `A6IB1hIEwCw` |

## remove | `_ZN7WebCore12NamedNodeMap3refEv` | `9GasEzC4uFE` |

## remove | `_ZN7WebCore13GraphicsLayer12setZPositionEf` | `KrJLG5ck2IA` |

## remove | `_ZN7WebCore13HitTestResultC1ERKNS_15HitTestLocationE` | `g5S0kerYDgw` |

## remove | `_ZN7WebCore13HitTestResultC1Ev` | `IfIiHBio398` |

## remove | `_ZN7WebCore13HitTestResultC2ERKNS_15HitTestLocationE` | `mQwyBMwgRRw` |

## remove | `_ZN7WebCore13HitTestResultC2Ev` | `WqDGv5ILjDI` |

## remove | `_ZN7WebCore13JSDOMRectListC1ERKS0_` | `zOySpeRm7w8` |

## remove | `_ZN7WebCore13JSDOMRectListC2ERKS0_` | `yzAdSdSXLLE` |

## remove | `_ZN7WebCore14DocumentLoader21setCustomHeaderFieldsEON3WTF6VectorINS_18CustomHeaderFieldsELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `pjVGJ0B1cfo` |

## remove | `_ZN7WebCore14ResourceHandle11clearClientEv` | `xCU2xwTBJP8` |

## remove | `_ZN7WebCore14ResourceHandle12firstRequestEv` | `j4kIUS5T3TY` |

## remove | `_ZN7WebCore15GraphicsContext20endTransparencyLayerEv` | `yCBKPL04RH0` |

## remove | `_ZN7WebCore15GraphicsContext22beginTransparencyLayerEf` | `hMCy3h9Z-ME` |

## remove | `_ZN7WebCore15HitTestLocationC1ERKS0_` | `WaA+akXnqK4` |

## remove | `_ZN7WebCore15HitTestLocationC1Ev` | `FtC9uKqXDjg` |

## remove | `_ZN7WebCore15HitTestLocationC2ERKS0_` | `oliwAKLVWgM` |

## remove | `_ZN7WebCore15HitTestLocationC2Ev` | `7rCiCCIa8Jo` |

## remove | `_ZN7WebCore15JSFetchResponseC1ERKS0_` | `XJT39Czi22Q` |

## remove | `_ZN7WebCore15JSFetchResponseC2ERKS0_` | `X2YwmzkrrcQ` |

## remove | `_ZN7WebCore16ScriptController11windowProxyEv` | `EgwcFZpfkbQ` |

## remove | `_ZN7WebCore16VisibleSelectionC1Ev` | `M2dlUKAGMNM` |

## remove | `_ZN7WebCore16VisibleSelectionC2Ev` | `erxK6j2uncY` |

## remove | `_ZN7WebCore17JSDOMRectReadOnlyC1ERKS0_` | `KW5JnX-5g-M` |

## remove | `_ZN7WebCore17JSDOMRectReadOnlyC2ERKS0_` | `pX-zIvHeGYI` |

## remove | `_ZN7WebCore18TextureMapperLayer10setOpacityEf` | `yn2JoBJXOik` |

## remove | `_ZN7WebCore18TextureMapperLayer11setPositionERKNS_10FloatPointE` | `z9VhBzk67Yw` |

## remove | `_ZN7WebCore18TextureMapperLayer12setTransformERKNS_20TransformationMatrixE` | `ETBRnmr8FCA` |

## remove | `_ZN7WebCore18TextureMapperLayer14setAnchorPointERKNS_12FloatPoint3DE` | `490GyRIQAu0` |

## remove | `_ZN7WebCore18TextureMapperLayer14setPreserves3DEb` | `s7YLdo2-V3E` |

## remove | `_ZN7WebCore18TextureMapperLayer15setContentsRectERKNS_9FloatRectE` | `iaaAR8GaGaU` |

## remove | `_ZN7WebCore18TextureMapperLayer15setDrawsContentEb` | `hP0LBvbf3IY` |

## remove | `_ZN7WebCore18TextureMapperLayer16setMasksToBoundsEb` | `Kky+pXcoHFY` |

## remove | `_ZN7WebCore18TextureMapperLayer17setContentsOpaqueEb` | `3MoxakHN9Eg` |

## remove | `_ZN7WebCore18TextureMapperLayer17setRepaintCounterEbi` | `1RO3nBwi2W4` |

## remove | `_ZN7WebCore18TextureMapperLayer18setContentsVisibleEb` | `4YzcZ4koEWo` |

## remove | `_ZN7WebCore18TextureMapperLayer19setContentsTileSizeERKNS_9FloatSizeE` | `AO0LHG1yvPw` |

## remove | `_ZN7WebCore18TextureMapperLayer20setChildrenTransformERKNS_20TransformationMatrixE` | `K7YUr62EN+E` |

## remove | `_ZN7WebCore18TextureMapperLayer20setContentsTilePhaseERKNS_9FloatSizeE` | `rvjIztKWq64` |

## remove | `_ZN7WebCore18TextureMapperLayer21setBackfaceVisibilityEb` | `wvS9v2o0piQ` |

## remove | `_ZN7WebCore18TextureMapperLayer23setContentsClippingRectERKNS_16FloatRoundedRectE` | `VrIbNi47c+o` |

## remove | `_ZN7WebCore18TextureMapperLayer7setSizeERKNS_9FloatSizeE` | `uD5cnILClQk` |

## remove | `_ZN7WebCore19JSAnimationTimelineC1ERKS0_` | `A6xEYmcRiho` |

## remove | `_ZN7WebCore19JSAnimationTimelineC2ERKS0_` | `sEgWSAmTMvc` |

## remove | `_ZN7WebCore19JSDOMMatrixReadOnlyC1ERKS0_` | `Bm7i18g1Vaw` |

## remove | `_ZN7WebCore19JSDOMMatrixReadOnlyC2ERKS0_` | `vHWd4Z92rhQ` |

## remove | `_ZN7WebCore20PasteboardCustomData5EntryC1Ev` | `8q0o+-HUPBE` |

## remove | `_ZN7WebCore20PasteboardCustomData5EntryC2Ev` | `GAOGiBWWxD0` |

## remove | `_ZN7WebCore20PasteboardCustomDataC1EON3WTF6StringEONS1_6VectorINS0_5EntryELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `ec46hZwJups` |

## remove | `_ZN7WebCore20PasteboardCustomDataC1EOS0_` | `71MqbV1TZQA` |

## remove | `_ZN7WebCore20PasteboardCustomDataC1Ev` | `cSUfCJp7YEw` |

## remove | `_ZN7WebCore20PasteboardCustomDataC2EON3WTF6StringEONS1_6VectorINS0_5EntryELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `y-m7ObwVY-8` |

## remove | `_ZN7WebCore20PasteboardCustomDataC2EOS0_` | `HuHO7Bn9W5k` |

## remove | `_ZN7WebCore20PasteboardCustomDataC2Ev` | `8htex21zoHw` |

## remove | `_ZN7WebCore20ScrollingCoordinator13pageDestroyedEv` | `2Nlpj99LvVo` |

## remove | `_ZN7WebCore21InspectorFrontendHost16disconnectClientEv` | `-1X3PhkFDT8` |

## remove | `_ZN7WebCore21InspectorFrontendHostC1EPNS_23InspectorFrontendClientEPNS_4PageE` | `-KdbpcGgEXU` |

## remove | `_ZN7WebCore21InspectorFrontendHostC2EPNS_23InspectorFrontendClientEPNS_4PageE` | `Sdm1lxP+5yA` |

## remove | `_ZN7WebCore24CoordinatedGraphicsLayer21invalidateCoordinatorEv` | `ultFJQj8-yY` |

## remove | `_ZN7WebCore26ContextDestructionObserver16contextDestroyedEv` | `yDx4ChtWlr8` |

## remove | `_ZN7WebCore27CSSComputedStyleDeclaration3refEv` | `p46F89e+PsY` |

## remove | `_ZN7WebCore27PlatformMediaSessionManager12restrictionsENS_20PlatformMediaSession9MediaTypeE` | `OJnJ+0Hi9S0` |

## remove | `_ZN7WebCore27PlatformMediaSessionManager14addRestrictionENS_20PlatformMediaSession9MediaTypeEj` | `mUy-LEiEf9k` |

## remove | `_ZN7WebCore4Page19addLayoutMilestonesEN3WTF9OptionSetINS_15LayoutMilestoneEEE` | `hn6OmDjk0ao` |

## remove | `_ZN7WebCore4Page22removeLayoutMilestonesEN3WTF9OptionSetINS_15LayoutMilestoneEEE` | `R3nFNsfiGUQ` |

## remove | `_ZN7WebCore4Page24setCORSDisablingPatternsEON3WTF6VectorINS_21UserContentURLPatternELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `Vi48XDpvR-M` |

## remove | `_ZN7WebCore4Page29startTrackingRenderingUpdatesEv` | `SQ9HlRcwJac` |

## remove | `_ZN7WebCore4Page34setInitialScaleIgnoringContentSizeEf` | `EWOfML0pnaI` |

## remove | `_ZN7WebCore4Page45setOutsideViewportThrottlingEnabledForTestingEb` | `AEG-wbnO1Yc` |

## remove | `_ZN7WebCore6Editor7CommandC1Ev` | `WPZPyKB1qYY` |

## remove | `_ZN7WebCore6Editor7CommandC2Ev` | `SjNoAv1glVM` |

## remove | `_ZN7WebCore6JSFileC1ERKS0_` | `fsTc6ANOby8` |

## remove | `_ZN7WebCore6JSFileC2ERKS0_` | `2YXmevi4vQ0` |

## remove | `_ZN7WebCore6Widget12setFrameRectERKNS_7IntRectE` | `PcOXjZjsab4` |

## remove | `_ZN7WebCore7CSSRule10setCssTextERKN3WTF6StringE` | `ix+R6upTWRo` |

## remove | `_ZN7WebCore7JSRangeC1ERKS0_` | `UkhNxEGudWI` |

## remove | `_ZN7WebCore7JSRangeC2ERKS0_` | `G2kjkUnsxJ4` |

## remove | `_ZN7WebCore8Document16setXMLStandaloneEb` | `k7OvvamK5A0` |

## remove | `_ZN7WebCore8Document25startTrackingStyleRecalcsEv` | `ID6l3yU4Gwg` |

## remove | `_ZN7WebCore8JSDOMURLC1ERKS0_` | `4gN-r7nYdYY` |

## remove | `_ZN7WebCore8JSDOMURLC2ERKS0_` | `MJ8jW45b-Ws` |

## remove | `_ZN7WebCore8JSPath2DC1ERKS0_` | `NEWQfiblmOM` |

## remove | `_ZN7WebCore8JSPath2DC2ERKS0_` | `cu+gVDIlA4w` |

## remove | `_ZN7WebCore9JSDOMRectC1ERKS0_` | `VhwtC33fcrw` |

## remove | `_ZN7WebCore9JSDOMRectC2ERKS0_` | `eSmlvBGD3QM` |

## remove | `_ZN9Inspector14ConsoleMessage14incrementCountEv` | `uIIZREKsTQU` |

## remove | `_ZN9Inspector22InspectorDebuggerAgent20setSuppressAllPausesEb` | `Po-3sCgpU9c` |

## remove | `_ZN9Inspector22RemoteInspectionTargetaSERKS0_` | `yWefaqPBkOE` |

## remove | `_ZN9Inspector24RemoteControllableTargetaSERKS0_` | `IO2yE8-du0I` |

## remove | `_ZNK3JSC4Yarr17RegularExpression13matchedLengthEv` | `TWnxOQaUhBI` |

## remove | `_ZNK3WTF10StringView16GraphemeClusters8IteratoreqERKS2_` | `DYcMsG69kBo` |

## remove | `_ZNK7WebCore11FrameLoader20activeDocumentLoaderEv` | `qv3QDQALDk0` |

## remove | `_ZNK7WebCore11MediaPlayer6volumeEv` | `qOGvhm61g-E` |

## remove | `_ZNK7WebCore11RenderStyle10lineHeightEv` | `mjaR1-gcxBM` |

## remove | `_ZNK7WebCore11RenderStyle11fontCascadeEv` | `yyrCVUOCeZo` |

## remove | `_ZNK7WebCore11RenderStyle15fontDescriptionEv` | `ZPuAH1rVJ7k` |

## remove | `_ZNK7WebCore14ResourceHandle6clientEv` | `Cgh0i6SAWZo` |

## remove | `_ZNK7WebCore16HTMLImageElement11cachedImageEv` | `sYj1Nwq78yQ` |

## remove | `_ZNK7WebCore16HTMLImageElement36pendingDecodePromisesCountForTestingEv` | `FuHXBscOxGk` |

## remove | `_ZNK7WebCore16HTMLImageElement8completeEv` | `T7zncgvFweM` |

## remove | `_ZNK7WebCore16HTMLInputElement10isURLFieldEv` | `XypQPgXg0KE` |

## remove | `_ZNK7WebCore16HTMLInputElement11isDateFieldEv` | `WHqhzve17Eg` |

## remove | `_ZNK7WebCore16HTMLInputElement11isTextFieldEv` | `TeE+ktl+Cw8` |

## remove | `_ZNK7WebCore16HTMLInputElement11isTimeFieldEv` | `BLylHdCcD2o` |

## remove | `_ZNK7WebCore16HTMLInputElement11isWeekFieldEv` | `MUlXrcCBGO0` |

## remove | `_ZNK7WebCore16HTMLInputElement12isEmailFieldEv` | `mDwiQvBlU90` |

## remove | `_ZNK7WebCore16HTMLInputElement12isFileUploadEv` | `vZikpIeFVMY` |

## remove | `_ZNK7WebCore16HTMLInputElement12isMonthFieldEv` | `I4q+6sukCEc` |

## remove | `_ZNK7WebCore16HTMLInputElement13isNumberFieldEv` | `VV5P3L0DrP0` |

## remove | `_ZNK7WebCore16HTMLInputElement13isSearchFieldEv` | `wd+PTkXXAKQ` |

## remove | `_ZNK7WebCore16HTMLInputElement15isPasswordFieldEv` | `wda1xC2CEiM` |

## remove | `_ZNK7WebCore16HTMLInputElement16isTelephoneFieldEv` | `y5jp97KAQuQ` |

## remove | `_ZNK7WebCore16HTMLInputElement20isDateTimeLocalFieldEv` | `1G5Prf1bClU` |

## remove | `_ZNK7WebCore16HTMLInputElement6isTextEv` | `w1CdZU2Srwo` |

## remove | `_ZNK7WebCore16HTMLMediaElement12playbackRateEv` | `TWe9e+Jmwz4` |

## remove | `_ZNK7WebCore16HTMLMediaElement27webkitClosedCaptionsVisibleEv` | `MtuVljq6Sq8` |

## remove | `_ZNK7WebCore16HTMLMediaElement6pausedEv` | `5uxtaxT7+2U` |

## remove | `_ZNK7WebCore16HTMLMediaElement6volumeEv` | `uAKuqBgtD-Q` |

## remove | `_ZNK7WebCore16HTMLMediaElement7seekingEv` | `FEWhRykt9L0` |

## remove | `_ZNK7WebCore16JSXMLHttpRequest7wrappedEv` | `kQEacF2vg-k` |

## remove | `_ZNK7WebCore17JSDOMGlobalObject13worldIsNormalEv` | `mS8j1EcK2tk` |

## remove | `_ZNK7WebCore17MouseRelatedEvent1xEv` | `B+anQG1XI-A` |

## remove | `_ZNK7WebCore18TextureMapperLayer10hasFiltersEv` | `VLD5cTfiN0o` |

## remove | `_ZNK7WebCore18TextureMapperLayer12drawsContentEv` | `EXRZRH444-o` |

## remove | `_ZNK7WebCore18TextureMapperLayer18contentsAreVisibleEv` | `QpeaElnNU7k` |

## remove | `_ZNK7WebCore18TextureMapperLayer7opacityEv` | `sWGGR-emFt0` |

## remove | `_ZNK7WebCore18TextureMapperLayer9layerRectEv` | `og6IDmhKqO4` |

## remove | `_ZNK7WebCore18TextureMapperLayer9transformEv` | `gj02Pc41dZk` |

## remove | `_ZNK7WebCore21RenderLayerCompositor15rootRenderLayerEv` | `oYipfXRE1HQ` |

## remove | `_ZNK7WebCore23FrameLoaderStateMachine15firstLayoutDoneEv` | `YAoMMSxEIkE` |

## remove | `_ZNK7WebCore23FrameLoaderStateMachine23committingFirstRealLoadEv` | `mUNgfT4aGQs` |

## remove | `_ZNK7WebCore23FrameLoaderStateMachine28creatingInitialEmptyDocumentEv` | `1Zy6+G3PD0M` |

## remove | `_ZNK7WebCore23FrameLoaderStateMachine32isDisplayingInitialEmptyDocumentEv` | `CeSQvxUUIYs` |

## remove | `_ZNK7WebCore24CachedResourceHandleBase3getEv` | `1yNctDJdXNM` |

## remove | `_ZNK7WebCore24CachedResourceHandleBasecvMS0_PNS_14CachedResourceEEv` | `uTd181B3Y58` |

## remove | `_ZNK7WebCore24CachedResourceHandleBasentEv` | `LwiybUx9yTw` |

## remove | `_ZNK7WebCore24CoordinatedGraphicsLayer17usesContentsLayerEv` | `pf2fwfamoG0` |

## remove | `_ZNK7WebCore26HTMLTextFormControlElement26isInnerTextElementEditableEv` | `8F-6OAVX8Ao` |

## remove | `_ZNK7WebCore28InspectorFrontendClientLocal28userInterfaceLayoutDirectionEv` | `d20WUyyiSr8` |

## remove | `_ZNK7WebCore4Node13ownerDocumentEv` | `z2YMNmnyqa0` |

## remove | `_ZNK7WebCore4Page15visibilityStateEv` | `9QZluZ4dNDY` |

## remove | `_ZNK7WebCore4Page23isMonitoringWheelEventsEv` | `Akm3oZiReFc` |

## remove | `_ZNK7WebCore6JSNode8nodeTypeERN3JSC14JSGlobalObjectE` | `e9NdD5ijfyA` |

## remove | `_ZNK7WebCore9SampleMap5emptyEv` | `Rg7MLXyCmtU` |

## remove | `_ZNK9Inspector14ConsoleMessage6sourceEv` | `tRhekl227Io` |

## remove | `_ZNK9Inspector22InspectorDebuggerAgent17breakpointsActiveEv` | `Fpchv+-9IbQ` |

## remove | `_ZNK9Inspector22InspectorDebuggerAgent8isPausedEv` | `Jgg+mJLsHHs` |
