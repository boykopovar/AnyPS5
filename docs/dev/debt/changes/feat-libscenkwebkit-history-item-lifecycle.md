# libSceNKWebKit

## change [WebCore::HistoryItem](../../../core/libs/prx/libSceNKWebKit/src/HistoryItemAbi.hpp)

- [WebCore::HistoryItem](../../../core/libs/prx/libSceNKWebKit/src/HistoryItemAbi.hpp) - layout follows pinned [HistoryItem.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/history/HistoryItem.h) (release, non-Cocoa, non-iOS, 8-bit `ShouldOpenExternalURLsPolicy`, 4-byte `PruningReason`) and is not checked against PS5: the 280-byte size, identifier at 176 and `std::optional<PolicyContainer>` at 192 (flag at 272, `optional` taken as value then `bool`; #18830) come from 11.x headers. The first two [CachedPage](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/history/CachedPage.h) fields are assumed to be a page pointer and `MonotonicTime` double at 0 and 8; expiry compares with `WTF::MonotonicTime::now`. Closes when: the size in `copy` or the identifier store is read from a firmware module, or measured on a console.

## add Functional

- [WebCore::HistoryItem lifecycle](../../../core/libs/prx/libSceNKWebKit/src/HistoryItem.cpp) - not compared with a console module. The constructors, destructor and `copy` follow pinned [HistoryItem.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/history/HistoryItem.cpp); like its copy constructor, `copy` leaves `documentState`, `shouldOpenExternalURLsPolicy`, `wasRestoredFromSession`, `wasCreatedByJSWithoutUserInteraction`, `shouldRestoreScrollPosition`, `stateObject`, `cachedPage` and the policy container at defaults. `copy` throws for `formData` in the item or a child, a child listed twice or in a cycle, and nesting over 4096. The destructor throws, before changing anything, for `stateObject`, `formData`, `cachedPage`, an engaged policy container or an atom or symbol string it would free. Guest `operator new` and `fastMalloc` use is unchecked. `url` and `originalURL` throw: they need a guest `WTF::URL` parser.

## change [WebCore::Process identifier](../../../core/libs/prx/libSceNKWebKit/src/WebCoreProcess.cpp)

- [WebCore::Process identifier](../../../core/libs/prx/libSceNKWebKit/src/WebCoreProcess.cpp) (libSceNKWebKit) - the generation-protection check is not modelled: in firmware 13.42 `identifier()` (`0x207100`) crashes before generating when the byte at `0x3369ef8` is 1, and no store to that byte by address was found in the module's code, so what sets it is unknown. The object-identifier counter it draws from is shared in the module with seven other exports and internal code, none of which is implemented, so identifiers are unique only among implemented callers. The rest follows `system_ex/common_ex/lib/libSceNKWebKit.sprx` 13.42 (SHA-256 `e63acdc164592330a287cf6914c265336b27a357098a449c1609f8f82e104409`; `setIdentifier` at `0x2e3bb0`), read from disassembly, not run. Closes when: measured on a console.

## remove | `_ZN7WebCore11HistoryItemC1Ev` | `G9Hp-BbB1Cs` |

## remove | `_ZN7WebCore11HistoryItemC2Ev` | `p9lalSG5Ois` |

## remove | `_ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_` | `5AfAYG74PQE` |

## remove | `_ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_` | `OWbX4UakOPI` |

## remove | `_ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_S4_` | `QBp4IOFhbaA` |

## remove | `_ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_` | `4ZjyKz4IjQo` |

## remove | `_ZN7WebCore11HistoryItemD1Ev` | `ahAkwbH9gio` |

## remove | `_ZN7WebCore11HistoryItemD2Ev` | `D3lMEqYuTXc` |

## remove | `_ZNK7WebCore11HistoryItem4copyEv` | `keZTgaBCdFs` |
