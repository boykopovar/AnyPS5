# libSceNKWebKit

## add Functional

- [ContainerNode and Node element navigation](../../../core/libs/prx/libSceNKWebKit/src/ElementNavigation.cpp) (libSceNKWebKit) - the six exports follow the pinned [ContainerNode.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/dom/ContainerNode.cpp#L914-L1032) and [Node.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/dom/Node.cpp#L508-L516). The child links (0x50, 0x58), sibling links (0x30, 0x38) and element bit (0x8 of the flags at 0x1c) of [NodeAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/NodeAbi.hpp) were read from the module with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d`; the other `Node` fields are not compared with a console module.

## remove | `_ZNK7WebCore13ContainerNode17firstElementChildEv` | `pllJ6xDOUj8` |

## remove | `_ZNK7WebCore13ContainerNode16lastElementChildEv` | `fGDYHA+uG4g` |

## remove | `_ZNK7WebCore13ContainerNode17childElementCountEv` | `u0hSyMPmgr4` |

## remove | `_ZNK7WebCore13ContainerNode17traverseToChildAtEj` | `3Af3+T3MOdY` |

## remove | `_ZNK7WebCore4Node18nextElementSiblingEv` | `AK1Vjnd7Yuk` |

## remove | `_ZNK7WebCore4Node22previousElementSiblingEv` | `Fg7DTuZhN2U` |
