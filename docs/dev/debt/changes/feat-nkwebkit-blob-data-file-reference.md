# libSceNKWebKit

## add Functional

- [WebCore::BlobDataFileReference](../../../core/libs/prx/libSceNKWebKit/src/BlobDataFileReference.cpp) (libSceNKWebKit) - not compared with a console module. Ported from [BlobDataFileReference.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/platform/network/BlobDataFileReference.cpp) at the 2021 revision whose ABI the module's WTF exports match, without `FILE_REPLACEMENT`. The 48-byte layout, vtable order and hidden-pointer `Optional<WallTime>` return were measured on a Linux build of that revision. File metadata follows that revision's unexported `fileMetadataFollowingSymlinks` over the guest `stat`: whole-second modification time, length only for regular files. The destructor deletes the replacement file as that revision's `FileSystem::deleteFile` does. A null object or path throws.

## remove | `_ZN7WebCore21BlobDataFileReference16revokeFileAccessEv` | `rNxUdFXvbmY` |

## remove | `_ZN7WebCore21BlobDataFileReference20prepareForFileAccessEv` | `xw7nZtsLETs` |

## remove | `_ZN7WebCore21BlobDataFileReference26startTrackingModificationsEv` | `d38462nDjJg` |

## remove | `_ZN7WebCore21BlobDataFileReference4pathEv` | `omzWoelkO8E` |

## remove | `_ZN7WebCore21BlobDataFileReference6createERKN3WTF6StringES4_` | `NKg-qME4uN0` |

## remove | `_ZN7WebCore21BlobDataFileReferenceC1ERKN3WTF6StringES4_` | `3u-CRxg2b0Q` |

## remove | `_ZN7WebCore21BlobDataFileReferenceC2ERKN3WTF6StringES4_` | `ngT2rbyKTXc` |

## remove | `_ZN7WebCore21BlobDataFileReferenceD0Ev` | `XGP2MNQH0o8` |

## remove | `_ZN7WebCore21BlobDataFileReferenceD1Ev` | `-Zst47j36u4` |

## remove | `_ZN7WebCore21BlobDataFileReferenceD2Ev` | `98EKVFkCQDI` |
