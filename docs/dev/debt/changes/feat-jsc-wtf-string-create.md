# libSceJsc

## change [Additional callable exports]

- [Additional callable exports](../../../core/libs/prx/libSceJsc/Export.cpp) - 278 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceJsc.c) and the [selected export metadata](https://gist.github.com/kyon-eth/e699987524d8485a9de9740280cfe4c1/09c89f4d64a17da12485e0eb637620ced4f6eeb2) from two version-labelled images (SHA-256 `d2a5f8a8807f23cb71170a5d3b779a57636f5a3480d92955e4255545ffe1e97e`). The labels are not authenticated. Apart from the 13 register-set queries and constructors and the two `Deprecated::ScriptValue` getters in the firmware 13.42 entries below, prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality. Shared WebKit exports require separate loader analysis.

## add Functional

- [WTF string creation](../../../core/libs/prx/libSceJsc/src/WtfStringCreate.cpp) - the console WTF version, allocator and layouts are unverified. libSceJsc exports no `StringImpl::destroy` or `fastFree`; guest code frees these strings through libSceNKWebKit's `StringImpl::destroy`, which works only because both use the same allocation header. Dropping the last reference of an atom, symbol or external-buffer string throws. Non-Latin-1 whitespace is the fixed Unicode `WS` bidi set, not the console ICU table. `split` returns a `Vector<String>` whose capacity equals its size. The 35 exports follow pinned [StringImpl.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/text/StringImpl.cpp) and [WTFString.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/text/WTFString.cpp). Null arguments, lengths WebKit rejects with `CRASH()`, an empty `split` separator and base 0 throw.

## remove | `_ZN3WTF10StringImpl17createFromLiteralEPKcj` | `eiR-c3WYWAE` |

## remove | `_ZN3WTF10StringImpl17createFromLiteralEPKc` | `mLHS0Q4GV-o` |

## remove | `_ZN3WTF10StringImpl18simplifyWhiteSpaceEv` | `6kEVUeU2EL8` |

## remove | `_ZN3WTF10StringImpl20create8BitIfPossibleEPKDs` | `2BtaIvBG7-Y` |

## remove | `_ZN3WTF10StringImpl20createWithoutCopyingEPKDsj` | `XVb+0NN54gc` |

## remove | `_ZN3WTF10StringImpl20createWithoutCopyingEPKhj` | `M0yz1LPGSaE` |

## remove | `_ZN3WTF10StringImpl6createEPKh` | `rUSjmJ0daUk` |

## remove | `_ZN3WTF10StringImpl7replaceEDsPS0_` | `PoT9sdk4N18` |

## remove | `_ZN3WTF10StringImpl7replaceEPS0_S1_` | `djdfuFWlVPI` |

## remove | `_ZN3WTF10StringImpl7replaceEjjPS0_` | `MejItkj1J4c` |

## remove | `_ZN3WTF10nullStringEv` | `z7ey00+6y+Q` |

## remove | `_ZN3WTF11emptyStringEv` | `yDG0A0GSJX0` |

## remove | `_ZN3WTF6String23make16BitFrom8BitSourceEPKhm` | `1dq4iQIG88U` |

## remove | `_ZN3WTF6String23make8BitFrom16BitSourceEPKDsm` | `I29SrxImX-A` |

## remove | `_ZN3WTF6String6appendEDs` | `K51HUUV9AWA` |

## remove | `_ZN3WTF6String6appendEPKDsj` | `iSHhSAr1jks` |

## remove | `_ZN3WTF6String6appendEPKhj` | `1IIKnMD2xqM` |

## remove | `_ZN3WTF6String6appendERKS0_` | `qEvRt3Wki8E` |

## remove | `_ZN3WTF6String6appendEh` | `4AO3zU38Gd8` |

## remove | `_ZN3WTF6String6insertERKS0_j` | `pFS2eHSDMLU` |

## remove | `_ZN3WTF6String6removeEjj` | `6jZLakbLGTI` |

## remove | `_ZN3WTF6String8truncateEj` | `ONNjULAZbZo` |

## remove | `_ZN3WTF6StringC1ENS_12ASCIILiteralE` | `URX9kwFwkis` |

## remove | `_ZN3WTF6StringC1EPKDs` | `HB6rCHl8fMs` |

## remove | `_ZN3WTF6StringC1EPKh` | `xN7BBNHgLrs` |

## remove | `_ZN3WTF6StringC2EPKDs` | `SE8wOZcQFuc` |

## remove | `_ZN3WTF6StringC2EPKh` | `XXiJQqFMwl8` |

## remove | `_ZNK3WTF6String15stripWhiteSpaceEv` | `gQ0v9iNBI44` |

## remove | `_ZNK3WTF6String18simplifyWhiteSpaceEv` | `QYuJju6ucf8` |

## remove | `_ZNK3WTF6String25splitAllowingEmptyEntriesERKS0_` | `A3YOM+5fr5A` |

## remove | `_ZNK3WTF6String33stripLeadingAndTrailingCharactersEPFbDsE` | `E5qSFrM0WtE` |

## remove | `_ZNK3WTF6String5splitERKS0_` | `i1zdtddvwdE` |

## remove | `_ZNK3WTF6String9substringEjj` | `aTSGWfAX47k` |

# libSceNKWebKit

## change [WTF and WebCore value layouts]

- [WTF and WebCore value layouts](../../../core/libs/prx/libSceNKWebKit/src/WtfStringAbi.hpp) (libSceNKWebKit) - `WTF::String`, `StringImpl`, `StringView`, `CString`, `URL` and the WebCore geometry, transform and `Color` layouts follow the release 64-bit headers at the [pinned source](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/StringImpl.h) and are pinned with `static_assert`; they are tested on Windows, not against console binaries. `StringImpl::destroy` frees internal, owned and substring strings with `fastFree`, including ones libSceJsc creates, which share the allocation header; atom and symbol strings and external buffers throw from it, and the host-side release terminates on them. `Expected<unsigned, HTMLIntegerParsingError>` follows the pinned `Expected.h`; its console ABI is unverified. The `MappedFileData` pointer-and-unsigned layout follows the pinned source, not a measured console object.

## change [WebCore::CookieUtil::defaultPathForURL]

- [WebCore::CookieUtil::defaultPathForURL](../../../core/libs/prx/libSceNKWebKit/src/CookieUtil.cpp) - console URL/String layouts and sret ABI remain unverified. Follows [CookieUtil.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/network/curl/CookieUtil.cpp#L184-L198): the existing URL path StringView export excludes query/fragment and yields an empty view for invalid URLs. Empty or non-slash-leading paths return `/`; otherwise the last slash and suffix are removed, with `/` when that slash is first. Path and prefix copies preserve character width; fallback StringImpls borrow static literal bytes. Host ABI release is balanced; guest `StringImpl::destroy` frees these impls. Tests construct the URL layout already on main, without unmerged parser exports. Closes when: compared with a console module.

## remove | `_ZN3WTF10StringImpl7destroyEPS0_` | `yxwlKktzy6Y` |
