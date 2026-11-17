# libSceNKWebKit

## add Functional

- [ISOBox and ISOFullBox parsing](../../../core/libs/prx/libSceNKWebKit/src/IsoBoxParsing.cpp) (libSceNKWebKit) - not compared with a console module. The 3.20-only `parse`, `peekBox` and `ISOFullBox::operator=` also follow pinned 11.x [ISOBox.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/iso/ISOBox.cpp), which clamps box sizes to the bytes left where 2021 WebKit did not. `JSC::DataView` reads follow [ArrayBufferView.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/ArrayBufferView.h), including the resizable branch that returns length 0 for in-bounds views. Objects get a host vtable `[D1, D0, parse]`. An in-range `uuid` extended type throws where WebKit's `flipBytes` reaches `RELEASE_ASSERT_NOT_REACHED`; a null argument throws. Closes when: compared with a console module.

## remove | `_ZN7WebCore10ISOFullBoxC1ERKS0_` | `zCgYva0hGps` |

## remove | `_ZN7WebCore10ISOFullBoxC1Ev` | `m3TcEKpIGo4` |

## remove | `_ZN7WebCore10ISOFullBoxC2ERKS0_` | `GMDlBS2FjfA` |

## remove | `_ZN7WebCore10ISOFullBoxC2Ev` | `Bw+7-OwsnYM` |

## remove | `_ZN7WebCore6ISOBox4readERN3JSC8DataViewE` | `B1GJfvSoSCA` |

## remove | `_ZN7WebCore6ISOBoxC1ERKS0_` | `gt1iXQ4Er+w` |

## remove | `_ZN7WebCore6ISOBoxC1Ev` | `TiRnQB6S6dE` |

## remove | `_ZN7WebCore6ISOBoxC2ERKS0_` | `KZF4xGSQH5c` |

## remove | `_ZN7WebCore6ISOBoxC2Ev` | `WU9IUHm2Kgs` |

## remove | `_ZN7WebCore6ISOBoxD0Ev` | `Fz+So9vLtw0` |

## remove | `_ZN7WebCore6ISOBoxD1Ev` | `OP+iChnwfIE` |

## remove | `_ZN7WebCore6ISOBoxD2Ev` | `VWJtvXhQaUY` |
