# libSceNKWebKit

## add Functional

- [Position constructors and containerNode](../../../core/libs/prx/libSceNKWebKit/src/PositionAnchor.cpp) (libSceNKWebKit) - the two anchor constructors (C1 and C2) and `containerNode` follow the pinned [Position.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/dom/Position.cpp#L117-L182). The [PositionAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/PositionAbi.hpp) layout (anchor node 0, offset 0x8, anchor type in the low three bits of the byte at 0xc) and the node reference increment of 2 at 0x18 were read from the module with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d`. The constructor taking a legacy editing flag is not implemented here.

## remove | `_ZN7WebCore8PositionC1EPNS_4NodeENS0_10AnchorTypeE` | `fJDljVbG4e4` |

## remove | `_ZN7WebCore8PositionC2EPNS_4NodeENS0_10AnchorTypeE` | `V2JbrOdI4B0` |

## remove | `_ZN7WebCore8PositionC1EPNS_4NodeEjNS0_10AnchorTypeE` | `lkKJnS1gDRk` |

## remove | `_ZN7WebCore8PositionC2EPNS_4NodeEjNS0_10AnchorTypeE` | `RJ+khi-q6YY` |

## remove | `_ZNK7WebCore8Position13containerNodeEv` | `-d0d1BTY40s` |
