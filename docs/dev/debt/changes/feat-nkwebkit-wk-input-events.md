# libSceNKWebKit

## add Unknown function info

- [PlayStation input events](../../../core/libs/prx/libSceNKWebKit/src/Event.cpp) (libSceNKWebKit) - the `WKKeyboardEvent`, `WKMouseEvent` and `WKWheelEvent` layouts (64, 32 and 64 bytes, returned through a hidden result pointer) and the three `Make` functions follow [WKEventPlayStation.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/Shared/API/c/neko/WKEventPlayStation.h) and [WKEventPlayStation.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebKit/Shared/API/c/neko/WKEventPlayStation.cpp); they are not in the upstream WebKit revision [c55364d5](https://github.com/WebKit/WebKit/tree/c55364d53f971f63a709976f168d932d558b8909), and the console module version and layouts are unverified. The `Convert`, `Accept` and `Cancel` key identifiers that composition input types store point to constant strings in the host module, not in the console module.

## change The 109 remaining

- The 108 remaining WebCore, WebKit C API, JavaScriptCore, Inspector and WTF exports in [Export.cpp](../../../core/libs/prx/libSceNKWebKit/Export.cpp) are present in the PS5 3.20 `libSceNKWebKit` export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Each is named after the symbol the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for its NID, and the NID of that name matches. Prototypes and behavior are unverified; each uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`. These declarations make no ABI or behavior claim.

## remove | `WKMouseEventMake` | `4LPi8JtvxYQ` |
