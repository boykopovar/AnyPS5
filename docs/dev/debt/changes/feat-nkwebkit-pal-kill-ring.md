# libSceNKWebKit

## add Functional

- [PAL::KillRing](../../../core/libs/prx/libSceNKWebKit/src/PalKillRing.cpp) (libSceNKWebKit) - not compared with a console module. The five exports follow the non-Mac definitions of pinned [KillRing.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/PAL/pal/text/KillRing.cpp#L29-L56), which the PlayStation port builds ([PlatformPlayStation.cmake](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/PAL/pal/PlatformPlayStation.cmake#L7)): `append`, `prepend`, `startNewSequence` and `setToYankedState` do nothing and `yank` returns a null `String`, so the ring never holds text. Closes when: compared with a console module.

## remove | `_ZN3PAL8KillRing16setToYankedStateEv` | `Zys2AhUVVN4` |

## remove | `_ZN3PAL8KillRing16startNewSequenceEv` | `OVuwx4R7Rxc` |

## remove | `_ZN3PAL8KillRing4yankEv` | `0zIN2tg-XUM` |

## remove | `_ZN3PAL8KillRing6appendERKN3WTF6StringE` | `XfqOSandGfA` |

## remove | `_ZN3PAL8KillRing7prependERKN3WTF6StringE` | `OOTZze9igVc` |
