# libSceNKWebKit

## add Functional

- [WebCore::CustomHeaderFields](../../../core/libs/prx/libSceNKWebKit/src/CustomHeaderFields.cpp) (libSceNKWebKit) - not compared with a console module. The two-vector layout (`fields` of `HTTPHeaderField`, then `thirdPartyDomains` of `String`) follows the pinned [CustomHeaderFields.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/CustomHeaderFields.h); the constructors, copy, assignment and destructor are the implicit ones, with copies on the guest heap at capacity equal to size. A null object or source throws. `thirdPartyDomainsMatch` stays a placeholder: it compares against `RegistrableDomain`, which the PS5 port computes with the console's public suffix list (`SceVshPsl`).

## remove | `_ZN7WebCore18CustomHeaderFieldsC1ERKS0_` | `D7s7dVKRTtU` |

## remove | `_ZN7WebCore18CustomHeaderFieldsC1Ev` | `WxG8d887NM8` |

## remove | `_ZN7WebCore18CustomHeaderFieldsC2ERKS0_` | `9iAZcHT7FBg` |

## remove | `_ZN7WebCore18CustomHeaderFieldsC2Ev` | `wQpMj4CVRs8` |

## remove | `_ZN7WebCore18CustomHeaderFieldsD2Ev` | `qQzi4cx9FYc` |

## remove | `_ZN7WebCore18CustomHeaderFieldsaSERKS0_` | `eYCzCuWMibg` |
