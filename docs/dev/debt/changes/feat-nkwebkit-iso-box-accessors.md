# libSceNKWebKit

## add Functional

- [ISO box accessors](../../../core/libs/prx/libSceNKWebKit/src/IsoBoxAccessors.cpp) (libSceNKWebKit) - not compared with a console module. The field offsets of `ISOBox`, `ISOFullBox`, `ISOTrackEncryptionBox`, `ISOProtectionSystemSpecificHeaderBox` and `ISOWebVTTCue` follow the pinned [ISOBox.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/iso/ISOBox.h) and its subclasses, measured with `offsetof` on a Linux build of those headers; `std::optional<int8_t>` is read as the value byte then the engaged byte. Vector getters return copies on the guest heap with capacity equal to size, as WTF's copy constructor does, `keyIDs` copying each key ID. A null object throws. Constructors, destructors, `parse`, `peekBox`, `read` and `peekSystemID` stay placeholders: the boxes are dispatched through the module's own vtables and parse a `JSC::DataView`, neither of which this library provides.

## remove | `_ZNK7WebCore21ISOTrackEncryptionBox10defaultKIDEv` | `yBSbIidwuO0` |

## remove | `_ZNK7WebCore21ISOTrackEncryptionBox17defaultConstantIVEv` | `JqeGWVkKzPY` |

## remove | `_ZNK7WebCore21ISOTrackEncryptionBox18defaultIsProtectedEv` | `5D2GzAMcjL4` |

## remove | `_ZNK7WebCore21ISOTrackEncryptionBox22defaultPerSampleIVSizeEv` | `qtSwAppE7YY` |

## remove | `_ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox4dataEv` | `xpYOrYjAoUc` |

## remove | `_ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox6keyIDsEv` | `xpSP40U+pqc` |

## remove | `_ZNK7WebCore36ISOProtectionSystemSpecificHeaderBox8systemIDEv` | `Gi7XC+1X+8o` |
