# libSceNKWebKit

## add Functional

- [DOMRectList construction and destruction](../../../core/libs/prx/libSceNKWebKit/src/DOMRectList.cpp) (libSceNKWebKit) - unverified against a console module: the 24-byte list (refCount at 0, vector at 8) and 48-byte DOMRect (wrapper, refCount, four doubles) follow the pinned DOMRectList.h and DOMRectReadOnly.h, and items come from `fastMalloc` where the console uses the DOMRectReadOnly IsoHeap. Otherwise the four constructors and the destructor follow [DOMRectList.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/dom/DOMRectList.cpp#L33-L44): refCount 1, null wrapper, `FloatQuad::boundingBox` boxes or the rects, destructor releases every item and frees the buffer. `length`, `item` and `create` are unexported, so item values stay unobservable. Null list, source or size-without-buffer throws `std::invalid_argument` where the console faults.

## remove | `_ZN7WebCore11DOMRectListC1ERKN3WTF6VectorINS_9FloatQuadELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `kOwgVpE5FIs` |

## remove | `_ZN7WebCore11DOMRectListC1ERKN3WTF6VectorINS_9FloatRectELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `s8Ld3Pj8+1M` |

## remove | `_ZN7WebCore11DOMRectListC2ERKN3WTF6VectorINS_9FloatQuadELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `lGp2LofF+9I` |

## remove | `_ZN7WebCore11DOMRectListC2ERKN3WTF6VectorINS_9FloatRectELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE` | `z9vqrN1CUBU` |

## remove | `_ZN7WebCore11DOMRectListD1Ev` | `MeT81dgytRg` |

## remove | `_ZN7WebCore11DOMRectListD2Ev` | `ckm37QV1V8c` |
