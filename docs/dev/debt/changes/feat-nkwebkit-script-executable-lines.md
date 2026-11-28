# libSceNKWebKit

## add Unknown function info

- [ScriptExecutable lastLine and endColumn](../../../core/libs/prx/libSceNKWebKit/src/ScriptExecutableLines.cpp) - every field offset is read from the module disassembly, not a layout model, and the function executable type value is 0xc where the pinned [JSType.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSType.h) has 0xb. Null pointers throw where the module would fault. Closes when: the executable class layouts are modelled from more than these two reads.

## remove | `_ZNK3JSC16ScriptExecutable8lastLineEv` | `OzE-5G8NTN8` |

## remove | `_ZNK3JSC16ScriptExecutable9endColumnEv` | `68VnGYcPyAc` |
