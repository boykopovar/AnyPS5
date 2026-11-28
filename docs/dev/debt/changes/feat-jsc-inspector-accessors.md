# libSceJsc

## add Functional

- [Inspector async call identifiers, remote target flags and saved result alias](../../../core/libs/prx/libSceJsc/src/InspectorAccessors.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version and layouts are unverified. They follow pinned [InspectorDebuggerAgent.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/JavaScriptCore/inspector/agents/InspectorDebuggerAgent.cpp#L403-L406) and the inline getters of its inspector headers: `asyncCallIdentifier` returns the 8-byte type and callback id pair in RAX, both remote target flags are read at offset 12 (after the vtable pointer and 32-bit target identifier, `USE(CF)` off), and `savedResultAlias` returns the `Optional<String>` at offset 24 of `InjectedScriptHost`. A null `this` throws. Closes when: compared with a console module.

## remove | `_ZN9Inspector22InspectorDebuggerAgent19asyncCallIdentifierENS0_13AsyncCallTypeEi` | `KhymfIzaujY` |

## remove | `_ZNK9Inspector18InjectedScriptHost16savedResultAliasEv` | `1EYcTwBoE4w` |

## remove | `_ZNK9Inspector22RemoteInspectionTarget22remoteDebuggingAllowedEv` | `nYbNPPMMWik` |
