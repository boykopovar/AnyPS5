# libSceNKWebKit

## add Functional

- [Inspector protocol BindingTraits assertions](../../../core/libs/prx/libSceNKWebKit/src/InspectorBindingTraits.cpp) (libSceNKWebKit) - the 20 `Inspector::Protocol::BindingTraits<T>::assertValueHasExpectedType(JSON::Value*)` exports do nothing: in WebKit they are `ASSERT`-only checks, and in the `libSceNKWebKit.sprx` with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` (the module the Constant-return exports entry reads as 11.00) each one is a single `ret` (release build, assertions compiled out).

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime12EntryPreviewEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `SHKtkCDxH6c` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime12RemoteObject4TypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `kY+dD6kxIkY` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime12RemoteObject7SubtypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `XWyKAz4u0EU` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime12RemoteObjectEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `a9quSKletss` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime13ObjectPreview4TypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `mTcMd-2kivA` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime13ObjectPreview7SubtypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `SFzo39L-Hgo` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime13ObjectPreviewEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `mtEs2TQIWSo` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime15CollectionEntryEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `k-+aF1Coi9E` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime15PropertyPreview4TypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `VIk8LuJJOU0` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime15PropertyPreview7SubtypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `4yC41GMuUq8` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime15PropertyPreviewEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `BsnnhuudXLE` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime18PropertyDescriptorEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `ZVoZm66RCz8` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_7Runtime26InternalPropertyDescriptorEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `6LjLZQePY74` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_8Debugger15FunctionDetailsEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `GOo1xl+bTjQ` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_8Debugger5Scope4TypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `bAjTIXtsWIY` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_8Debugger5ScopeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `I84EUs864es` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_8Debugger8LocationEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `PBc4LFKaGVw` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_8Debugger9CallFrameEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `87mnHfWxrbQ` |

## remove | `_ZN9Inspector8Protocol13BindingTraitsINS0_8Timeline9EventTypeEE26assertValueHasExpectedTypeEPN3WTF8JSONImpl5ValueE` | `8aN1sGR1vfg` |
