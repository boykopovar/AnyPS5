# libSceNKWebKit

## add Functional

- [JSC::CellAttributes::dump and JSC::DOMJIT::HeapRange::dump](../../../core/libs/prx/libSceNKWebKit/src/JscDumps.cpp) (libSceNKWebKit) - not compared with a console module; the 2-byte `CellAttributes` (`int8_t` `DestructionMode`, `int8_t` `HeapCell::Kind`) and 4-byte `HeapRange` (`uint16_t` begin, end) layouts and the enum values follow the pinned [CellAttributes.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/heap/CellAttributes.h#L34-L47) and are unverified. `CellAttributes::dump` prints `{`, the mode name, `, `, the kind name and `}` in one `print`, each name in its own nested `print`, as the pinned `printInternal` overloads do; a value outside those enums throws before anything is printed instead of the release assert. `HeapRange::dump` is the stream's `printf("0x%x-0x%x", begin, end)`. Closes when: compared with a console module.

## remove | `_ZNK3JSC14CellAttributes4dumpERN3WTF11PrintStreamE` | `X4pGci2dAvc` |

## remove | `_ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE` | `ti74yN+Sg8o` |
