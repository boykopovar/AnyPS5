# libSceNKWebKit

## add Functional

- [Vtable constructors](../../../core/libs/prx/libSceNKWebKit/src/VtableConstructors.cpp) (libSceNKWebKit) - 35 constructors and copy constructors whose body in the `libSceNKWebKit.sprx` with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` (the module the Constant-return exports entry reads as 11.00) only stores the class vtable and constant field values. The 17 vtables (344 slots) are rebuilt slot by slot from the module's relocations: 200 slots point to the export with that name, 123 to `__cxa_pure_virtual`, and the 21 internal `ret`, `xor eax,eax; ret` and null-result bodies are reproduced by local helpers. Offset-to-top and type info are 0, as in the module (no RTTI). Classes with any other slot body (`LoaderStrategy`, `MediaStrategy`, `SocketProvider`, `PageConsoleClient`, the filter operations and others) are left out. A null object throws.

## remove | `_ZN3JSC15WeakHandleOwnerC1ERKS0_` | `GZMY2InuGqM` |

## remove | `_ZN3JSC15WeakHandleOwnerC1Ev` | `CuJ1lpUapSs` |

## remove | `_ZN3JSC15WeakHandleOwnerC2ERKS0_` | `M+HQlnym0PY` |

## remove | `_ZN3JSC15WeakHandleOwnerC2Ev` | `ZsUOENm0Vc8` |

## remove | `_ZN3WTF18FunctionDispatcherC2Ev` | `YsRNfwMQmtQ` |

## remove | `_ZN7WebCore13MIMETypeCacheC1Ev` | `XQ79w+11C78` |

## remove | `_ZN7WebCore13MIMETypeCacheC2Ev` | `s6qnyKuSfU8` |

## remove | `_ZN7WebCore15XPathNSResolverC2Ev` | `FPwp7ypjzlc` |

## remove | `_ZN7WebCore16BlobRegistryImplC1Ev` | `o5z9VIc1MWQ` |

## remove | `_ZN7WebCore16BlobRegistryImplC2Ev` | `4aHyuLG09aw` |

## remove | `_ZN7WebCore16VisitedLinkStoreC2Ev` | `iIeJWwhbaG0` |

## remove | `_ZN7WebCore18PluginInfoProviderC2Ev` | `1XvItBGOHII` |

## remove | `_ZN7WebCore19UserContentProviderC2Ev` | `UtDLjo9rPmw` |

## remove | `_ZN7WebCore22EmptyFrameLoaderClientC1ERKS0_` | `E2pRXDTKB68` |

## remove | `_ZN7WebCore22EmptyFrameLoaderClientC1Ev` | `GcUPkO7ruRw` |

## remove | `_ZN7WebCore22EmptyFrameLoaderClientC2ERKS0_` | `YQZ9c5jMwJc` |

## remove | `_ZN7WebCore22EmptyFrameLoaderClientC2Ev` | `DrzwGvqQVZc` |

## remove | `_ZN7WebCore24StorageNamespaceProviderC2Ev` | `hMjn6z51jLI` |

## remove | `_ZN7WebCore27ContentSecurityPolicyClientC2ERKS0_` | `mDeup2ozJz4` |

## remove | `_ZN7WebCore27ContentSecurityPolicyClientC2Ev` | `dQ3p09wGMvQ` |

## remove | `_ZN7WebCore28InspectorFrontendClientLocal8SettingsC1ERKS1_` | `n8Re5ynSfjU` |

## remove | `_ZN7WebCore28InspectorFrontendClientLocal8SettingsC1Ev` | `WvNZUdSvf6E` |

## remove | `_ZN7WebCore28InspectorFrontendClientLocal8SettingsC2ERKS1_` | `1KYfLN5YfnI` |

## remove | `_ZN7WebCore28InspectorFrontendClientLocal8SettingsC2Ev` | `SrZqj8GEkmM` |

## remove | `_ZN9Inspector15RemoteInspector6ClientC2ERKS1_` | `fm5iMf1GSvg` |

## remove | `_ZN9Inspector15RemoteInspector6ClientC2Ev` | `VwdSDjyPJ6s` |

## remove | `_ZN9Inspector24RemoteControllableTargetC2ERKS0_` | `XRmIkQ5gbwA` |

## remove | `_ZN9Inspector24RemoteControllableTargetC2Ev` | `pngra5JlGpI` |
