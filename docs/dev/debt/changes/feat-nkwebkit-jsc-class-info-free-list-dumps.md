# libSceNKWebKit

## add Functional

- [JSC::ClassInfo::dump and JSC::FreeList::dump](../../../core/libs/prx/libSceNKWebKit/src/JscObjectDumps.cpp) (libSceNKWebKit) - not compared with a console module; `className` as the first `ClassInfo` field and the 40-byte `FreeList` (`intervalStart`, `intervalEnd`, `nextInterval`, `uint64_t` secret, `unsigned` originalSize, cellSize) follow the pinned [FreeList.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/heap/FreeList.h) and are unverified. `ClassInfo::dump` prints `this` with `%p`, `:[`, the name in its own nested `print` as `ASCIILiteral::dump` does, and `]`; a null `className` throws instead of printing it with `%s`. `FreeList::dump` prints the pinned fields with `%p`, `%lu` and `%u` in one `print`. Closes when: compared with a console module.

## remove | `_ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE` | `L9Hmp6AYcQU` |

## remove | `_ZNK3JSC8FreeList4dumpERN3WTF11PrintStreamE` | `w-pn1iTYQo0` |
