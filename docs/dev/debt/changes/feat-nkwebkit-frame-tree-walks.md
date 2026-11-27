# libSceNKWebKit

## add Functional

- [FrameTree childCount, top, traverseNext and isDescendantOf](../../../core/libs/prx/libSceNKWebKit/src/FrameTreeWalks.cpp) (libSceNKWebKit) - follow the pinned [FrameTree.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/FrameTree.cpp#L219-L378). The [FrameAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/FrameAbi.hpp) offsets read (`Frame` page 0x40 and tree 0x50, `FrameTree` parent 0x8, next sibling 0x20, first child 0x30) come from the module with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d`. `traverseNext(CanWrap, DidWrap*)` stays a stub: the module's `Frame` keeps the main frame at 0x38, which the pinned Frame.h does not.

## remove | `_ZNK7WebCore9FrameTree10childCountEv` | `NF1s99Jgzow` |

## remove | `_ZNK7WebCore9FrameTree3topEv` | `9J8+IeONNco` |

## remove | `_ZNK7WebCore9FrameTree12traverseNextEPKNS_5FrameE` | `ANRxBkkTRPg` |

## remove | `_ZNK7WebCore9FrameTree14isDescendantOfEPKNS_5FrameE` | `mdoGohzjCsY` |
