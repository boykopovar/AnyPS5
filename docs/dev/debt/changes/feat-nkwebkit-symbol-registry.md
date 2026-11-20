# libSceNKWebKit

## add Functional

- [WTF symbols and symbol registry](../../../core/libs/prx/libSceNKWebKit/src/WtfSymbolRegistry.cpp) (libSceNKWebKit) - the guest `SymbolRegistry` table is not modelled: its symbols live in a host table keyed by the registry's address, and the guest `HashSet` stays empty. Symbol creation, the 40- and 48-byte symbol layouts and the registry's strong references follow [SymbolImpl.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/SymbolImpl.cpp), [SymbolImpl.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/SymbolImpl.h) and [SymbolRegistry.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/SymbolRegistry.cpp) (PS5 11.x). Releasing a symbol no longer calls `std::terminate`.

## remove | `_ZN3WTF10SymbolImpl16createNullSymbolEv` | `mi8ipN3nDyg` |

## remove | `_ZN3WTF10SymbolImpl6createERNS_10StringImplE` | `24SDQYS0N4M` |

## remove | `_ZN3WTF14SymbolRegistry12symbolForKeyERKNS_6StringE` | `4eO+y6WtOIw` |

## remove | `_ZN3WTF14SymbolRegistryD1Ev` | `4wHuG6WpyF0` |

## remove | `_ZN3WTF14SymbolRegistryD2Ev` | `l8UKHjO29+w` |

## remove | `_ZN3WTF17PrivateSymbolImpl6createERNS_10StringImplE` | `S6NzIicbleU` |
