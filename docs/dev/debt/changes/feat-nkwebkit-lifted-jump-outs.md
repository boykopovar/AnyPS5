# libSceNKWebKit

## add Functional

- [Lifted jump-outs](../../../core/libs/prx/libSceNKWebKit/src/LiftedJumpOuts.cpp) (libSceNKWebKit) - 40 exports whose body in the `libSceNKWebKit.sprx` with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` (the module the Constant-return exports entry reads as 11.00) is a jump to another function of the module, either a named export or an internal function whose bytes were inlined, with loads, stores, arithmetic and calls to other named exports around it. Each body was translated instruction by instruction into C++ that keeps every call, argument register, load and store in order. Callees that are still stubs throw, so these exports only work once their callees do. A low (null-page) address throws instead of faulting. The `LiftedJumpOuts` test replays recorded call logs and memory digests on Linux and checks null objects everywhere. Parameter and return types are register widths, not source prototypes.

## remove | `WKAXObjectPage` | `KrVXrd3WyfM` |

## remove | `WKBundleFrameGetJavaScriptContext` | `eI2iByoOtYU` |

## remove | `WKBundleFrameGetPendingUnloadCount` | `4ZQbnnW1z+Q` |

## remove | `WKBundleFrameStopLoading` | `6yYwCC3Gfmo` |

## remove | `WKBundleGarbageCollectJavaScriptObjects` | `vNNCUDOFpFY` |

## remove | `WKBundleGarbageCollectJavaScriptObjectsOnAlternateThreadForDebugging` | `aus1pUvQ7ts` |

## remove | `WKBundlePageClearMainFrameName` | `iP5usIxGDAI` |

## remove | `WKBundlePageIsTrackingRepaints` | `viw0OkjLHgQ` |

## remove | `WKBundlePageListenForLayoutMilestones` | `l5JzEGDSvhk` |

## remove | `WKBundlePageOverlayClear` | `LVb-RgymeuY` |

## remove | `WKBundlePageResetTrackedRepaints` | `6IFQEEuM7Yg` |

## remove | `WKBundlePageSetAppCacheMaximumSize` | `sSb3te485KU` |

## remove | `WKBundlePageSetPolicyClient` | `kl9cnbrBzrA` |

## remove | `WKBundlePageSetTracksRepaints` | `K57m00RjFaQ` |

## remove | `WKBundleSetTabKeyCyclesThroughElements` | `t7K3DznwwOU` |

## remove | `WKContextSetConnectionClient` | `S4qCVFS2p1Y` |

## remove | `WKPageGetContext` | `fLfh+wZ12vQ` |

## remove | `WKViewSetViewAccessibilityClient` | `IMJ+G6b6cx0` |

## remove | `WKWebsiteDataStoreGetResourceLoadStatisticsEnabled` | `2H0q+W0RyYk` |

## remove | `_ZN3JSC21MarkedJSValueRefArrayD1Ev` | `nYh9OiczTZM` |

## remove | `_ZN3WTF24AutomaticThreadConditionD1Ev` | `eW8DnRSDucM` |

## remove | `_ZN3WTF24AutomaticThreadConditionD2Ev` | `qjebiZKT83w` |

## remove | `_ZN7WebCore13GraphicsLayer18setBackgroundColorERKNS_5ColorE` | `8dEZbWDA6-4` |

## remove | `_ZN7WebCore18TextureMapperLayer13setSolidColorERKNS_5ColorE` | `9aG1Y+TwE4k` |

## remove | `_ZN7WebCore4Page31setRemoteInspectionNameOverrideERKN3WTF6StringE` | `isdbBZoLdo4` |

## remove | `_ZN7WebCore8Document56hasRequestedPageSpecificStorageAccessWithUserInteractionERKNS_17RegistrableDomainE` | `URxENN44a6k` |

## remove | `_ZN7WebCore8Settings23setUsesBackForwardCacheEb` | `TlX0LQPNYFs` |

## remove | `_ZN7WebCore8Settings24setVisualViewportEnabledEb` | `Wxxk5GbijWc` |

## remove | `_ZN7WebCore8Settings32setAcceleratedCompositingEnabledEb` | `9rw-dYdn3bA` |

## remove | `_ZN7WebCore8Settings32setShouldAllowUserInstalledFontsEb` | `VsjLgu8EHe8` |

## remove | `_ZN7WebCore8Settings33setFontFallbackPrefersPictographsEb` | `JKsefjJ5H7A` |

## remove | `_ZN7WebCore8Settings41setAcceleratedCompositedAnimationsEnabledEb` | `1Iv-JJC7joU` |

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_9ImageDataE` | `aFJnQE4CaWw` |

## remove | `_ZNK7WebCore13HitTestResult14innerNodeFrameEv` | `sngv+7kclEc` |

## remove | `_ZNK7WebCore18DOMWindowExtension5frameEv` | `Xy24q3qh8eQ` |

## remove | `_ZNK7WebCore20ScrollingCoordinator36coordinatesScrollingForOverflowLayerERKNS_11RenderLayerE` | `1nxJkpm8wLA` |

## remove | `_ZNK7WebCore4Node7baseURIEv` | `IoGwVQHItWY` |
