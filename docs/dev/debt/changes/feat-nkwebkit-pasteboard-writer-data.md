# libSceNKWebKit

## add Functional

- [WebCore::PasteboardWriterData](../../../core/libs/prx/libSceNKWebKit/src/PasteboardWriterData.cpp) (libSceNKWebKit) - not compared with a console module. The 88-byte object of three `std::optional`s follows the pinned [PasteboardWriterData.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/PasteboardWriterData.h) for a port that is neither Cocoa nor GTK (empty `WebContent`, `URLData` of a `URL` and a title), measured on a Linux build of that header; each optional is taken as its value followed by an engaged byte. The constructor only clears the engaged bytes and the destructor destroys the engaged members in reverse order. A null object throws.

## remove | `_ZN7WebCore20PasteboardWriterDataC1Ev` | `KhG5XEHZYsM` |

## remove | `_ZN7WebCore20PasteboardWriterDataC2Ev` | `H-PxK0DNH70` |

## remove | `_ZN7WebCore20PasteboardWriterDataD1Ev` | `EDHPIv3WKb0` |

## remove | `_ZN7WebCore20PasteboardWriterDataD2Ev` | `A9MS1-aXvKI` |

## remove | `_ZNK7WebCore20PasteboardWriterData7isEmptyEv` | `pCjXnlN6R4c` |
