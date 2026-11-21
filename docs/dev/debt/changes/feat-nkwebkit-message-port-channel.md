# libSceNKWebKit

## add Functional

- [MessagePortChannel processForPort and hasAnyMessagesPendingOrInFlight](../../../core/libs/prx/libSceNKWebKit/src/MessagePortChannel.cpp) (libSceNKWebKit) - the two exports follow the pinned [MessagePortChannel.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/dom/messageports/MessagePortChannel.cpp#L63-L69). The `MessagePortChannel` offsets of [MessagePortChannelAbi.hpp](../../../core/libs/prx/libSceNKWebKit/src/MessagePortChannelAbi.hpp) (ports at 0x8 and 0x18, process slots at 0x30 and 0x40 as a value plus an engaged byte, pending message vector sizes at 0x6c and 0x7c, batches in flight at 0xa0) were read from the module with SHA-256 `3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d` and match the pinned MessagePortChannel.h; the other fields are laid out from the pinned header and are not compared with a console module.

## remove | `_ZNK7WebCore18MessagePortChannel31hasAnyMessagesPendingOrInFlightEv` | `F0fNBmtdOgo` |
