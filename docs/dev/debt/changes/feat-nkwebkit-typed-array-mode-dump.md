# libSceNKWebKit

## add Functional

- [WTF::printInternal for JSC::TypedArrayMode](../../../core/libs/prx/libSceNKWebKit/src/TypedArrayModeDump.cpp) (libSceNKWebKit) - the console enum encoding, signature and `PrintStream` vtable are unverified. The byte argument and 12 names follow pinned PS5 WebKit 11.x [JSArrayBufferView.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSArrayBufferView.h#L58-L97) and [JSArrayBufferView.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSArrayBufferView.cpp#L389-L432), using `begin`, `vprintf` and `end` slots 4, 2 and 5. Unknown modes throw before callbacks instead of asserting. Unreadable streams or vtables, null callbacks and a null stream returned by `begin` throw as host-safety fallbacks. No console or title comparison.

## remove | `_ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE` | `DwwpqTcKG0Q` |
