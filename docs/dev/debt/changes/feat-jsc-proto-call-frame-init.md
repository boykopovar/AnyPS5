# libSceJsc

## add Functional

- [JSC::ProtoCallFrame init, needArityCheck and codeBlock](../../../core/libs/prx/libSceJsc/src/ProtoCallFrame.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version is unverified. They follow pinned [ProtoCallFrame.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/interpreter/ProtoCallFrame.h#L35-L84), [ProtoCallFrameInlines.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/interpreter/ProtoCallFrameInlines.h#L33-L50) and [StackAlignment.h](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/runtime/StackAlignment.h#L44-L47) on the 56-byte frame of the libSceNKWebKit accessors, `hasArityMismatch` at byte 36. `init` stores the pointers and `this` as given, the count with a zero tag, and the count rounded up to odd modulo 2^32 as `paddedArgCount`. A non-null `CodeBlock`, which only a VM creates, and a null frame throw. Closes when: compared with a console module.

## remove | `_ZN3JSC14ProtoCallFrame14needArityCheckEv` | `R4cvOCertOM` |

## remove | `_ZN3JSC14ProtoCallFrame4initEPNS_9CodeBlockEPNS_14JSGlobalObjectEPNS_8JSObjectENS_7JSValueEiPS7_` | `tVCtmnhUdkg` |
