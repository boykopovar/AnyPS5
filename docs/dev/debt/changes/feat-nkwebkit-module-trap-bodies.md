# libSceNKWebKit

## add Functional

- [Trap-body exports](../../../core/libs/prx/libSceNKWebKit/src/TrapBodies.cpp) (libSceNKWebKit) - 47 exports whose entire body in the `libSceNKWebKit.sprx` with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` (the module the Constant-return exports entry reads as 11.00) is a trap, `ud2; ud2` or `int3; ud2`, with no other bytes in the symbol. They end the process through `std::terminate` instead of throwing `NotImplemented`. 46 are deleting or complete destructors (`D0`/`D1`, and the `D0` thunks of `InspectorAuditAgent`, `InspectorRuntimeAgent` and `InspectorDebuggerAgent`), and one is `JSC::Identifier::checkCurrentAtomStringTable`, whose pinned [PS5-WebKit-11.x](https://github.com/neonmodder123/PS5-WebKit-11.x/tree/80f77e94c63ee7202eec3d4b5b1a021dd89011ba) release-build body is `CRASH()`.

## remove | `_ZN3JSC10Identifier27checkCurrentAtomStringTableERNS_2VME` | `951o4aXk+P8` |

## remove | `_ZN3JSC14JSRunLoopTimerD0Ev` | `xYHAR2W390o` |

## remove | `_ZN3JSC14SourceProviderD0Ev` | `4AeMF+7VG38` |

## remove | `_ZN3JSC17MarkingConstraintD0Ev` | `gGkAhbLKpaU` |

## remove | `_ZN3JSC18GCActivityCallbackD0Ev` | `4xSvkMgasQc` |

## remove | `_ZN3JSC18GCActivityCallbackD1Ev` | `VoyTsrKlfdg` |

## remove | `_ZN3JSC20TypedArrayControllerD0Ev` | `dfRy73zK7WU` |

## remove | `_ZN3JSC2VM10ClientDataD0Ev` | `aNZOcfGHsDI` |

## remove | `_ZN3JSC8Bindings8InstanceD0Ev` | `gPYiacKjOTw` |

## remove | `_ZN3WTF15AutomaticThreadD0Ev` | `WEVBe0m-ZBo` |

## remove | `_ZN3WTF18FunctionDispatcherD0Ev` | `5qqjEdxEeeU` |

## remove | `_ZN7WebCore11DisplayList8RecorderD0Ev` | `TNo4xzYkEW0` |

## remove | `_ZN7WebCore11ImageBufferD0Ev` | `jabcrdjwdUs` |

## remove | `_ZN7WebCore11ImageBufferD1Ev` | `84955-7DGwQ` |

## remove | `_ZN7WebCore13GraphicsLayerD0Ev` | `5Rj2MyJZATM` |

## remove | `_ZN7WebCore14LoaderStrategyD0Ev` | `EHrOTTgMxXo` |

## remove | `_ZN7WebCore14ScrollableAreaD0Ev` | `BVaird4y-R8` |

## remove | `_ZN7WebCore15ActiveDOMObjectD0Ev` | `mNIajby8IWQ` |

## remove | `_ZN7WebCore15XPathNSResolverD0Ev` | `MhXub7fXkKw` |

## remove | `_ZN7WebCore16VisitedLinkStoreD0Ev` | `mqQ1TbImxtg` |

## remove | `_ZN7WebCore18ImageBufferBackendD0Ev` | `suczT1Sa7kc` |

## remove | `_ZN7WebCore18PluginInfoProviderD0Ev` | `umRPVA-p9k4` |

## remove | `_ZN7WebCore19UserContentProviderD0Ev` | `aN4xuJLVnKc` |

## remove | `_ZN7WebCore21DisplayRefreshMonitorD0Ev` | `x-SSNEsxvWU` |

## remove | `_ZN7WebCore24StorageNamespaceProviderD0Ev` | `CT3th2KZ+4o` |

## remove | `_ZN7WebCore27ContentSecurityPolicyClientD0Ev` | `UoaDfV72Frw` |

## remove | `_ZN7WebCore27ContentSecurityPolicyClientD1Ev` | `cMEdJ1S+9Pk` |

## remove | `_ZN7WebCore28InspectorFrontendClientLocalD0Ev` | `2ZnBc9ZCIqw` |

## remove | `_ZN7WebCore6WidgetD0Ev` | `MXDmNHNZdHE` |

## remove | `_ZN7WebCore9TimerBaseD0Ev` | `YK76WadcCQI` |

## remove | `_ZN9Inspector15InspectorTargetD1Ev` | `GtEbqvEBuK4` |

## remove | `_ZN9Inspector15RemoteInspector6ClientD0Ev` | `qM6EjseJ7ro` |

## remove | `_ZN9Inspector19InspectorAuditAgentD0Ev` | `QRVGV6+ykOU` |

## remove | `_ZN9Inspector20InjectedScriptModuleD0Ev` | `TnCgJO3CzGw` |

## remove | `_ZN9Inspector21InspectorRuntimeAgentD0Ev` | `ilvKlfjyUHo` |

## remove | `_ZN9Inspector22InspectorDebuggerAgentD0Ev` | `XMJPvWiQWd4` |

## remove | `_ZN9Inspector22RemoteAutomationTargetD0Ev` | `ewpJla+Xdkk` |

## remove | `_ZN9Inspector22RemoteInspectionTargetD1Ev` | `gw6eHbjfARY` |

## remove | `_ZN9Inspector24RemoteControllableTargetD0Ev` | `z1TlnsoaCKY` |

## remove | `_ZThn16_N9Inspector19InspectorAuditAgentD0Ev` | `OqPy5+E3JrY` |

## remove | `_ZThn16_N9Inspector21InspectorRuntimeAgentD0Ev` | `AqqoCHSoK7Y` |

## remove | `_ZThn16_N9Inspector22InspectorDebuggerAgentD0Ev` | `Bt4ps1bQCis` |

## remove | `_ZThn24_N9Inspector22InspectorDebuggerAgentD0Ev` | `U6TtqBmL14A` |
