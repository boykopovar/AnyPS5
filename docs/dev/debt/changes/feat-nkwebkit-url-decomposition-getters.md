# libSceNKWebKit

## add Functional

- [WebCore::URLDecomposition getters](../../../core/libs/prx/libSceNKWebKit/src/URLDecomposition.cpp) (libSceNKWebKit) - not compared with a console module. `fullURL` is called through vtable slot 2, after the complete and deleting destructors, from the declaration order of the pinned [URLDecomposition.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/html/URLDecomposition.h#L32-L69) as measured on a Linux build of that header; the subclass's URL is released after each call. The getters follow [URLDecomposition.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/html/URLDecomposition.cpp). `port` reads the digits after the host and returns an empty string when they are missing or do not fit 16 bits. A null object or vtable throws.

## remove | `_ZNK7WebCore16URLDecomposition4hashEv` | `djfNyVDLFz4` |

## remove | `_ZNK7WebCore16URLDecomposition4hostEv` | `yM6UFAsJJr4` |

## remove | `_ZNK7WebCore16URLDecomposition4portEv` | `gnsIZjIH-Mw` |

## remove | `_ZNK7WebCore16URLDecomposition6searchEv` | `lZybtDrBJ60` |

## remove | `_ZNK7WebCore16URLDecomposition8hostnameEv` | `sBYBAkdDGPY` |

## remove | `_ZNK7WebCore16URLDecomposition8pathnameEv` | `GfBvEsyC8oY` |

## remove | `_ZNK7WebCore16URLDecomposition8protocolEv` | `BwoYh9eiWvw` |
