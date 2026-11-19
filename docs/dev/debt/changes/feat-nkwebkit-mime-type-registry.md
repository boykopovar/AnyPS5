# libSceNKWebKit

## change [WebCore MIME type cache and media queries]

- [WebCore MIME type cache and media queries](../../../core/libs/prx/libSceNKWebKit/src/Mime.cpp) (libSceNKWebKit) - MIMETypeCache, supportedMediaMIMETypes, supportedNonImageMIMETypes and systemPreviewMIMETypes throw (needs mediaplayer.sprx, unpinned HashSet, PS5-only systemPreviewMIMETypes); isSupportedMediaMIMEType false for null/empty, throws otherwise. canShowMIMEType uses [MIMETypeRegistry.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/MIMETypeRegistry.cpp): image, non-image, JSON, text/ types answer without the media set (unsupported text/ false), others throw. extensionsForMIMEType returns an empty Vector, matching PlayStation release [ASSERT_NOT_IMPLEMENTED_YET](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/playstation/MIMETypeRegistryPlayStation.cpp#L86-L90); allowedFileExtensions trims one leading dot, dedups case-sensitively.

## remove | `_ZN7WebCore16MIMETypeRegistry15canShowMIMETypeERKN3WTF6StringE` | `HJ3vdF4LmxM` |

## remove | `_ZN7WebCore16MIMETypeRegistry21allowedFileExtensionsERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_` | `GnTXkxscE8k` |

## remove | `_ZN7WebCore16MIMETypeRegistry21extensionsForMIMETypeERKN3WTF6StringE` | `2iORhW4G1F4` |

## remove | `_ZN7WebCore16MIMETypeRegistry24isSupportedMediaMIMETypeERKN3WTF6StringE` | `RNAdd6DpntM` |
