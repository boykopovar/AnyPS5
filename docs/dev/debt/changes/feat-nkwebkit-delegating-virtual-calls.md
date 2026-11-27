# libSceNKWebKit

## add Functional

- [Delegating virtual calls](../../../core/libs/prx/libSceNKWebKit/src/DelegatingExports.cpp) (libSceNKWebKit) - 104 exports whose body in the `libSceNKWebKit.sprx` with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` (the module the Constant-return exports entry reads as 11.00) is `mov off(%rdi),%rdi; mov (%rdi),%rax; jmp *slot(%rax)`: they load a member pointer and tail-call one virtual function of that object, such as `MediaPlayer::m_private` or `m_client`. They are implemented as the same assembly tail jump with each export's offset and slot, so every argument and return register is passed through and no prototype is assumed. A null object or null member throws instead of faulting. Member offsets: `WebCore::MediaPlayer` at 0x50 (49), `WebCore::MediaPlayer` at 0x18 (36), `WebCore::HTMLInputElement` at 0xf8 (7), `WebCore::Chrome` at 0x10 (4), `WebCore::RenderLayerBacking` at 0x38 (2), `WebCore::FrameLoader` at 0x8 (2), `WebCore::BackForwardController` at 0x8 (2), `WebCore::CanvasRenderingContext` at 0x18 (1), `WebCore::Page` at 0x78 (1). The `MediaPlayer` offsets match `playerPrivate()` (0x50) and `client()` (0x18) in [FieldGetters.cpp](../../../core/libs/prx/libSceNKWebKit/src/FieldGetters.cpp). Four `WK*` C API exports with the same body are left out, because AnyPS5's own API objects do not use the module's layout.

## remove | `_ZN7WebCore11MediaPlayer10cancelLoadEv` | `ONXrD9F8ExY` |

## remove | `_ZN7WebCore11MediaPlayer11naturalSizeEv` | `dGAcIVHLm8o` |

## remove | `_ZN7WebCore11MediaPlayer11rateChangedEv` | `c-jBFhrvm2g` |

## remove | `_ZN7WebCore11MediaPlayer11sizeChangedEv` | `5KgmHsaCxXg` |

## remove | `_ZN7WebCore11MediaPlayer11timeChangedEv` | `kFkr8Q7gREk` |

## remove | `_ZN7WebCore11MediaPlayer12addTextTrackERNS_22InbandTextTrackPrivateE` | `jGEtfWnk1J4` |

## remove | `_ZN7WebCore11MediaPlayer12networkStateEv` | `3wBlLV8DiaY` |

## remove | `_ZN7WebCore11MediaPlayer13addAudioTrackERNS_17AudioTrackPrivateE` | `IoDV+oqRRWo` |

## remove | `_ZN7WebCore11MediaPlayer13tracksChangedEv` | `d++4nnx2Qr0` |

## remove | `_ZN7WebCore11MediaPlayer15durationChangedEv` | `2Vi3aJUlabI` |

## remove | `_ZN7WebCore11MediaPlayer15removeTextTrackERNS_22InbandTextTrackPrivateE` | `J5j1pBL1cjg` |

## remove | `_ZN7WebCore11MediaPlayer16removeAudioTrackERNS_17AudioTrackPrivateE` | `hJRPzfJPa34` |

## remove | `_ZN7WebCore11MediaPlayer16removeVideoTrackERNS_17VideoTrackPrivateE` | `mBE-rCYG6xg` |

## remove | `_ZN7WebCore11MediaPlayer17seekWithToleranceERKN3WTF9MediaTimeES4_S4_` | `8LJOLHmWey4` |

## remove | `_ZN7WebCore11MediaPlayer18liveUpdateIntervalEv` | `qQ96-aBFJz0` |

## remove | `_ZN7WebCore11MediaPlayer18mediaEngineUpdatedEv` | `Hcukgmlncpk` |

## remove | `_ZN7WebCore11MediaPlayer19syncTextTrackBoundsEv` | `uJUGbADcTTg` |

## remove | `_ZN7WebCore11MediaPlayer20cachedResourceLoaderEv` | `VJFsO+gjT+c` |

## remove | `_ZN7WebCore11MediaPlayer20playbackStateChangedEv` | `xgjt4Dkhfuo` |

## remove | `_ZN7WebCore11MediaPlayer20resourceNotSupportedEv` | `Zo-iipctpP0` |

## remove | `_ZN7WebCore11MediaPlayer21characteristicChangedEv` | `QWAd+Vrr4mY` |

## remove | `_ZN7WebCore11MediaPlayer22performTaskAtMediaTimeEON3WTF8FunctionIFvvEEERKNS1_9MediaTimeE` | `3SmmHWPetYc` |

## remove | `_ZN7WebCore11MediaPlayer24firstVideoFrameAvailableEv` | `Q2r4AKLMrac` |

## remove | `_ZN7WebCore11MediaPlayer24remoteEngineFailedToLoadEv` | `UoMZIQx7JiI` |

## remove | `_ZN7WebCore11MediaPlayer24setClosedCaptionsVisibleEb` | `NFMPkcE9feI` |

## remove | `_ZN7WebCore11MediaPlayer25bufferedTimeRangesChangedEv` | `-h9+qj63jl8` |

## remove | `_ZN7WebCore11MediaPlayer25seekableTimeRangesChangedEv` | `iovLyay-VDk` |

## remove | `_ZN7WebCore11MediaPlayer25shouldIgnoreIntrinsicSizeEv` | `7w0tVnYH3KI` |

## remove | `_ZN7WebCore11MediaPlayer26activeSourceBuffersChangedEv` | `3yfS8opxosg` |

## remove | `_ZN7WebCore11MediaPlayer26applicationDidBecomeActiveEv` | `AplSHRUidGA` |

## remove | `_ZN7WebCore11MediaPlayer26setTextTrackRepresentationEPNS_23TextTrackRepresentationE` | `GuGxzhc35rM` |

## remove | `_ZN7WebCore11MediaPlayer27applicationWillResignActiveEv` | `7ZGiKfb37l0` |

## remove | `_ZN7WebCore11MediaPlayer28setShouldMaintainAspectRatioEb` | `5cQZYmhq8JE` |

## remove | `_ZN7WebCore11MediaPlayer29acceleratedCompositingEnabledEv` | `zmIG3Jx3Cbc` |

## remove | `_ZN7WebCore11MediaPlayer32acceleratedRenderingStateChangedEv` | `tSM6+q9HRmU` |

## remove | `_ZN7WebCore11MediaPlayer34seekableTimeRangesLastModifiedTimeEv` | `Het7UaL5QvI` |

## remove | `_ZN7WebCore11MediaPlayer4playEv` | `tpslt5d45m4` |

## remove | `_ZN7WebCore11MediaPlayer4seekERKN3WTF9MediaTimeE` | `cmFnNND+sAo` |

## remove | `_ZN7WebCore11MediaPlayer5paintERNS_15GraphicsContextERKNS_9FloatRectE` | `047cYEcKtJA` |

## remove | `_ZN7WebCore11MediaPlayer5pauseEv` | `1HH1+iMik7U` |

## remove | `_ZN7WebCore11MediaPlayer7repaintEv` | `I55q7IbIU-A` |

## remove | `_ZN7WebCore11MediaPlayer7setRateEd` | `Hhki7ncbDjg` |

## remove | `_ZN7WebCore16HTMLInputElement11selectColorEN3WTF10StringViewE` | `WMmW8ChmYdE` |

## remove | `_ZN7WebCore16HTMLInputElement5filesEv` | `TEmyq2mz3QU` |

## remove | `_ZN7WebCore18RenderLayerBacking30setIsTrackingDisplayListReplayEb` | `EhRJ-yl2m9E` |

## remove | `_ZN7WebCore22CanvasRenderingContext5derefEv` | `ASfhwEyh1cI` |

## remove | `_ZN7WebCore4Page23clearUndoRedoOperationsEv` | `cyTN4IIukQU` |

## remove | `_ZN7WebCore6Chrome23enableSuddenTerminationEv` | `mkZZVt6In6c` |

## remove | `_ZN7WebCore6Chrome24disableSuddenTerminationEv` | `39tjjgU19JY` |

## remove | `_ZNK7WebCore11FrameLoader7frameIDEv` | `fB20kfX+6s0` |

## remove | `_ZNK7WebCore11MediaPlayer13isVideoPlayerEv` | `RoIWhEt0jTg` |

## remove | `_ZNK7WebCore11MediaPlayer13movieLoadTypeEv` | `EIdtyyQIVFE` |

## remove | `_ZNK7WebCore11MediaPlayer13platformLayerEv` | `BdmkDCjE3fE` |

## remove | `_ZNK7WebCore11MediaPlayer13requestedRateEv` | `m1Y7YpNH-OY` |

## remove | `_ZNK7WebCore11MediaPlayer16canSaveMediaDataEv` | `fg0HFN49oxg` |

## remove | `_ZNK7WebCore11MediaPlayer16supportsScanningEv` | `xnARf4a4prE` |

## remove | `_ZNK7WebCore11MediaPlayer17decodedFrameCountEv` | `nfc7UmDvD5k` |

## remove | `_ZNK7WebCore11MediaPlayer17doesHaveAttributeERKN3WTF10AtomStringEPS2_` | `DjWf03qkdoY` |

## remove | `_ZNK7WebCore11MediaPlayer17droppedFrameCountEv` | `dLwUVYV1jA0` |

## remove | `_ZNK7WebCore11MediaPlayer17hasClosedCaptionsEv` | `GMtQuGiE5MI` |

## remove | `_ZNK7WebCore11MediaPlayer18maxFastForwardRateEv` | `iJVourQXpgQ` |

## remove | `_ZNK7WebCore11MediaPlayer18minFastReverseRateEv` | `f1HAS+flwvE` |

## remove | `_ZNK7WebCore11MediaPlayer18shouldDisableSleepEv` | `rRCIxcuzMgs` |

## remove | `_ZNK7WebCore11MediaPlayer18supportsFullscreenEv` | `zUBEypuu+Qk` |

## remove | `_ZNK7WebCore11MediaPlayer19mediaCacheDirectoryEv` | `AMvdjcTxfbs` |

## remove | `_ZNK7WebCore11MediaPlayer19playerContentsScaleEv` | `6VJpFkVmh5g` |

## remove | `_ZNK7WebCore11MediaPlayer20playerContentBoxRectEv` | `KHCFjwCNxoQ` |

## remove | `_ZNK7WebCore11MediaPlayer20renderingModeChangedEv` | `Yd1Rm-5Quo0` |

## remove | `_ZNK7WebCore11MediaPlayer21audioDecodedByteCountEv` | `go8pfVYxUDw` |

## remove | `_ZNK7WebCore11MediaPlayer21videoDecodedByteCountEv` | `LgC+bA2tQUg` |

## remove | `_ZNK7WebCore11MediaPlayer22didPassCORSAccessCheckEv` | `xboOgWkcWVw` |

## remove | `_ZNK7WebCore11MediaPlayer22hasAvailableVideoFrameEv` | `5vyXyk7zR6M` |

## remove | `_ZNK7WebCore11MediaPlayer24shouldUsePersistentCacheEv` | `7QTMSPEe1lw` |

## remove | `_ZNK7WebCore11MediaPlayer24supportsPictureInPictureEv` | `SIibyT2wPaU` |

## remove | `_ZNK7WebCore11MediaPlayer25renderingCanBeAcceleratedEv` | `NkFH5mAgFjE` |

## remove | `_ZNK7WebCore11MediaPlayer26shouldCheckHardwareSupportEv` | `dbSDLYu3gzY` |

## remove | `_ZNK7WebCore11MediaPlayer28requiresImmediateCompositingEv` | `gCWqjdrnC9g` |

## remove | `_ZNK7WebCore11MediaPlayer28supportsAcceleratedRenderingEv` | `rlnpPNMzl70` |

## remove | `_ZNK7WebCore11MediaPlayer31maximumDurationToCacheMediaTimeEv` | `8rRzoDFknyk` |

## remove | `_ZNK7WebCore11MediaPlayer31requiresTextTrackRepresentationEv` | `12gexxIy+t0` |

## remove | `_ZNK7WebCore11MediaPlayer35platformVolumeConfigurationRequiredEv` | `XwQprd-SN94` |

## remove | `_ZNK7WebCore11MediaPlayer41mediaContentTypesRequiringHardwareSupportEv` | `gY2judYG38M` |

## remove | `_ZNK7WebCore11MediaPlayer4rateEv` | `uYpN3d-+c9o` |

## remove | `_ZNK7WebCore11MediaPlayer5endedEv` | `P-Zhj99ghLM` |

## remove | `_ZNK7WebCore11MediaPlayer6pausedEv` | `svFP+RQpUbk` |

## remove | `_ZNK7WebCore11MediaPlayer7seekingEv` | `tRPZufpBbYs` |

## remove | `_ZNK7WebCore11MediaPlayer8hasAudioEv` | `Y5pqmZuI9KA` |

## remove | `_ZNK7WebCore11MediaPlayer8hasVideoEv` | `PvP3AczVrzY` |

## remove | `_ZNK7WebCore11MediaPlayer9isLoopingEv` | `6EkiZ2KuQ1U` |

## remove | `_ZNK7WebCore16HTMLInputElement11valueAsDateEv` | `LTE1dovGMro` |

## remove | `_ZNK7WebCore16HTMLInputElement13valueAsNumberEv` | `rLsPBLktI-A` |

## remove | `_ZNK7WebCore16HTMLInputElement21autoFillButtonElementEv` | `dsKVK6wUloE` |

## remove | `_ZNK7WebCore16HTMLInputElement5widthEv` | `m2gmPnFLeck` |

## remove | `_ZNK7WebCore16HTMLInputElement6heightEv` | `PZEeC9wi0qo` |

## remove | `_ZNK7WebCore18RenderLayerBacking12tiledBackingEv` | `g7VJwWXQAP0` |

## remove | `_ZNK7WebCore21BackForwardController12forwardCountEv` | `wmkk8v-vEI0` |

## remove | `_ZNK7WebCore21BackForwardController9backCountEv` | `AZ+JE981Lgg` |
