# libSceNKWebKit

## add Functional

- [More field getters](../../../core/libs/prx/libSceNKWebKit/src/FieldGetters.cpp) (libSceNKWebKit) - 45 more exports whose body in the `libSceNKWebKit.sprx` with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` (the module the Constant-return exports entry reads as 11.00) is one load from `this` and `ret`: 34 one-byte loads into `al` (only the low byte is defined, so they return `std::uint8_t`), 2 `movzwl` 16-bit loads, 1 32-bit, 1 `double` (`vmovsd` into `xmm0`), 1 address (`lea`) and 6 64-bit loads. Offsets and widths are read from the bodies; field meanings are not checked.
- [More field setters](../../../core/libs/prx/libSceNKWebKit/src/FieldSetters.cpp) (libSceNKWebKit) - 19 more exports whose body in the same module is one store into `this` and `ret`: 11 store the low byte of the second argument, 5 store a constant byte (`stopRunningTasks`, `setDidGCRecently`, `enable`/`disableICECandidateFiltering`, `InspectorTarget::pause`), and 1, 1 and 1 store 16, 32 and 64 bits. A null object throws, as in the existing setters.

## remove | `_ZN3JSC17DeferredWorkTimer16stopRunningTasksEv` | `mc2Ulvzkoz8` |

## remove | `_ZN3JSC18GCActivityCallback10setEnabledEb` | `rU0ByKESekI` |

## remove | `_ZN3JSC22FullGCActivityCallback16setDidGCRecentlyEv` | `GNAx4p223bE` |

## remove | `_ZN7WebCore10ScrollView23setPaintsEntireContentsEb` | `kDHvtssgHfY` |

## remove | `_ZN7WebCore12EventHandler23setImmediateActionStageENS_20ImmediateActionStageE` | `wz5xdJm7GsY` |

## remove | `_ZN7WebCore15DatabaseManager14setIsAvailableEb` | `R9HHtazgMJY` |

## remove | `_ZN7WebCore18TextureMapperLayer5setIDEj` | `kS92Nwfsyrg` |

## remove | `_ZN7WebCore19InspectorController26setInspectorFrontendClientEPNS_23InspectorFrontendClientE` | `IWP1omvGMT8` |

## remove | `_ZN7WebCore21NetworkStorageSession31setThirdPartyCookieBlockingModeENS_28ThirdPartyCookieBlockingModeE` | `l22aiCQV8jY` |

## remove | `_ZN7WebCore4Page19setDelegatesScalingEb` | `3p1EBzuU7lg` |

## remove | `_ZN7WebCore4Page27enableICECandidateFilteringEv` | `KM3HTLCv0tk` |

## remove | `_ZN7WebCore4Page28disableICECandidateFilteringEv` | `ZPlZo-3S8XM` |

## remove | `_ZN7WebCore4Page37setInLowQualityImageInterpolationModeEb` | `yHZvUA01NKo` |

## remove | `_ZN7WebCore8Document24setShouldCreateRenderersEb` | `HZvInRgsr8M` |

## remove | `_ZN9Inspector15InspectorTarget5pauseEv` | `JGYMjGdyi+E` |

## remove | `_ZN9Inspector15RemoteInspector29waitingForAutomaticInspectionEj` | `1LXGEu7l1es` |

## remove | `_ZNK3JSC18GCActivityCallback9isEnabledEv` | `CAOt1d2AT44` |

## remove | `_ZNK3JSC22FullGCActivityCallback13didGCRecentlyEv` | `fuwxa+XM0rQ` |

## remove | `_ZNK3WTF15AutomaticThread19hasUnderlyingThreadERKNS_14AbstractLockerE` | `U7n0UvpCFFw` |

## remove | `_ZNK7WebCore10ScrollView14useFixedLayoutEv` | `Ev4YVLUDVe0` |

## remove | `_ZNK7WebCore11FrameLoader10isCompleteEv` | `P2sq9TH-fEA` |

## remove | `_ZNK7WebCore11FrameLoader8loadTypeEv` | `BbLWFSkBPRk` |

## remove | `_ZNK7WebCore11MediaPlayer14preservesPitchEv` | `Ch0rwJ8oqIA` |

## remove | `_ZNK7WebCore11MediaPlayer25preferredDynamicRangeModeEv` | `h1oT-Ke-wl0` |

## remove | `_ZNK7WebCore11MediaPlayer39contentMIMETypeWasInferredFromExtensionEv` | `ibqF5xhmbmI` |

## remove | `_ZNK7WebCore11MediaPlayer5mutedEv` | `hjULUs7uHiA` |

## remove | `_ZNK7WebCore11MediaPlayer7preloadEv` | `LsaZNLQQwUY` |

## remove | `_ZNK7WebCore11XPathResult10resultTypeEv` | `zWdJYlMai5M` |

## remove | `_ZNK7WebCore14DocumentLoader16mouseEventPolicyEv` | `5FVmKLxFnys` |

## remove | `_ZNK7WebCore15ProgressTracker17estimatedProgressEv` | `yKesHVodhLA` |

## remove | `_ZNK7WebCore16HTMLMediaElement15bufferingPolicyEv` | `Zo6ks32VHL4` |

## remove | `_ZNK7WebCore16HTMLMediaElement20webkitPreservesPitchEv` | `f7bEpmyxkiA` |

## remove | `_ZNK7WebCore17FullscreenManager21isAnimatingFullscreenEv` | `0nmzC8IaJ3s` |

## remove | `_ZNK7WebCore17FullscreenManager27areFullscreenControlsHiddenEv` | `240U4cfeeh8` |

## remove | `_ZNK7WebCore4Page34inLowQualityImageInterpolationModeEv` | `FUxAKQOPLHU` |

## remove | `_ZNK9Inspector14ConsoleMessage4typeEv` | `mX2cjtETZtI` |

## remove | `_ZNK9Inspector14ConsoleMessage5levelEv` | `gD32LzUw9AQ` |

## remove | `_ZNK9Inspector15InspectorTarget8isPausedEv` | `KrfpsXXdW-w` |

## remove | `_ZNK9Inspector15RemoteInspector21hasActiveDebugSessionEv` | `P0V8cbW+Z-E` |

## remove | `_ZNK9Inspector15RemoteInspector7enabledEv` | `iTANhsf3zHk` |

## remove | `_ZNK9Inspector21InspectorConsoleAgent7enabledEv` | `2IKm-BXcq9g` |

## remove | `_ZNK9Inspector22InspectorDebuggerAgent27pauseOnNextStatementEnabledEv` | `25lgQJ93JO4` |

## remove | `_ZNK9Inspector22InspectorDebuggerAgent7enabledEv` | `1DD+9cjt3hU` |

## remove | `_ZNK9Inspector22RemoteAutomationTarget8isPairedEv` | `ew8ebxoQMLo` |

## remove | `_ZNK9Inspector28InspectorScriptProfilerAgent18isAlreadyProfilingEv` | `tH-+IbGsGNE` |

## remove | `_ZThn24_NK9Inspector28InspectorScriptProfilerAgent18isAlreadyProfilingEv` | `W5zgLuJ-xXU` |
