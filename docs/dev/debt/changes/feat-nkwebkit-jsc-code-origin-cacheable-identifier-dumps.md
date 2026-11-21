# libSceNKWebKit

## add Functional

- [JSC::CodeOrigin::dump and JSC::CacheableIdentifier::dump](../../../core/libs/prx/libSceNKWebKit/src/JscBytecodeDumps.cpp) (libSceNKWebKit) - not compared with a console module; the 8-byte `CodeOrigin` composite word (48-bit address width) and `CacheableIdentifier` tagged word follow the pinned [CodeOrigin.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/bytecode/CodeOrigin.h#L203-L264) and [CacheableIdentifier.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/CacheableIdentifier.h#L102-L103) and are unverified. A non-inlined origin prints `bc#` and `cp#`; an inlined one throws, as `InlineCallFrame` is not modelled. A uid identifier prints its UTF-8 text; a cell one throws, as it needs `JSValue::dump`. Closes when: compared with a console module.

## remove | `_ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE` | `63bmb0nlVdw` |

## remove | `_ZNK3JSC19CacheableIdentifier4dumpERN3WTF11PrintStreamE` | `BER1Mhcfk28` |
