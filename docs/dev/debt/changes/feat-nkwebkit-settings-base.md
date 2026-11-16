# libSceNKWebKit

## add Functional

- [WebCore::SettingsBase font family layout](../../../core/libs/prx/libSceNKWebKit/src/SettingsBase.cpp) (libSceNKWebKit) - not compared with a console module. The console symbols take `const WTF::AtomString&` where pinned [SettingsBase.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/SettingsBase.h) declares `const String&`; both are an 8-byte `String` pointer. `fontGenericFamilies()` sits at vtable slots 0 and 1 before the destructor pair, per that header's declaration order; the seven `HashMap<int, String>` maps follow the pinned [HashTable.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/HashTable.h).

- [WebCore::SettingsBase font family changes](../../../core/libs/prx/libSceNKWebKit/src/SettingsBase.cpp) (libSceNKWebKit) - not compared with a console module. The setters throw `NotImplemented` before any map change when the value would change and do not modify it otherwise, instead of updating the map and running the unmodelled `FontCascadeCache` invalidation and, with a page, `setNeedsRecalcStyleInAllFrames()`. Upstream lookups treat script -2, the map's empty sentinel, as matching the first empty bucket, so upstream mutations with that script corrupt the table (keyCount underflow); the setters throw before changing anything and the getters return the bucket value without reproducing the corruption.

- [WebCore::SettingsBase timer and media content types](../../../core/libs/prx/libSceNKWebKit/src/SettingsBase.cpp) (libSceNKWebKit) - not compared with a console module. `setMinimumDOMTimerInterval` stores the `Seconds` value at offset 16 and throws `NotImplemented` before writing when the `WeakPtr<Page>` at offset 8 is non-null, instead of walking the page frames with `adjustMinimumDOMTimerInterval`. The media vector sits at offset 72, from the pinned member order (8-byte `WeakPtr<Page>`, 8-byte `Seconds`, 48-byte `Timer`); not measured on console. The media setters follow the pinned [Vector.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/Vector.h) copy assignment and append growth and `StringView::split(':')` skipping empty entries. `userPrefersSimplifiedChinese` recomputes on every call instead of caching and reads `SYSTEM_LANGUAGE` through the host `getenv_np`, whose overlong-value handling is unknown on console.

## remove | `_ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE` | `lC2+-6XMW38` |

## remove | `_ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE` | `305+7Bx1JWY` |

## remove | `_ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `uvEBlyPL0pU` |

## remove | `_ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode` | `voLOq3g1XTI` |

## remove | `_ZNK7WebCore12SettingsBase15serifFontFamilyE11UScriptCode` | `i4eT3QOzLNI` |

## remove | `_ZNK7WebCore12SettingsBase17cursiveFontFamilyE11UScriptCode` | `CMKyrGMW54Y` |

## remove | `_ZNK7WebCore12SettingsBase17fantasyFontFamilyE11UScriptCode` | `LYNNj5rNb7w` |

## remove | `_ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode` | `7RpkbtIp-o4` |

## remove | `_ZNK7WebCore12SettingsBase19sansSerifFontFamilyE11UScriptCode` | `5j4kGT4mYL8` |

## remove | `_ZNK7WebCore12SettingsBase20pictographFontFamilyE11UScriptCode` | `C9lhqCz3iZ0` |
