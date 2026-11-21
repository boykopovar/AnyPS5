# libSceHttp2

## add Unknown function info

- [libSceHttp2 synchronous WebSocket exports](../../../core/libs/prx/libSceHttp2/InventoryExports.cpp) - nothing gives the prototypes or results of `sceHttp2WebSocketClose`, `sceHttp2WebSocketSendTextMessage`, `sceHttp2WebSocketSendDataMessage` and `sceHttp2WebSocketSetCloseTimeout`. SharpProspero's `Http2.cs`, prosper and rps5e declare only the asynchronous twins and the ping setters. Dropping `asyncOption` from an asynchronous twin is not a sound derivation: `sceHttp2ReadData` and `sceHttp2SendRequest` each drop both trailing option arguments that their asynchronous forms add. A prototype longer than the console's would read an unset register as `option` and reject a valid call with 0x817B11FE, so all four still throw through `NotImplemented_nid_no_patch`. Closes when: a read of the 13.40 `libSceHttp2.sprx` exports or a title call site gives each prototype.
