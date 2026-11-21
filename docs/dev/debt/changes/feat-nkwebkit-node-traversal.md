# libSceNKWebKit

## add Functional

- [NodeTraversal deepLastChild, last and nextAncestorSibling](../../../core/libs/prx/libSceNKWebKit/src/NodeTraversal.cpp) (libSceNKWebKit) - the four exports follow the pinned [NodeTraversal.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/dom/NodeTraversal.cpp#L83-L122). The `Node` and `ContainerNode` offsets of [NodeAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/NodeAbi.hpp) (flags at 0x1c, parent at 0x20, next sibling at 0x38, last child at 0x58) were read from the module with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` and checked against the pinned Node.h and ContainerNode.h; the other `Node` fields are not compared with a console module.

## remove | `_ZN7WebCore13NodeTraversal13deepLastChildERNS_4NodeE` | `YMdfx4CK+Jo` |

## remove | `_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeE` | `E0o0m00RS1E` |

## remove | `_ZN7WebCore13NodeTraversal19nextAncestorSiblingERKNS_4NodeEPS2_` | `IaQoBoi2WTQ` |

## remove | `_ZN7WebCore13NodeTraversal4lastERKNS_13ContainerNodeE` | `uGjapTSgkJE` |
