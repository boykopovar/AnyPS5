# libSceIpmi technical debt

### Unknown function info

- [libSceIpmi pending exports](../../../core/libs/prx/libSceIpmi/PendingExports.cpp) - 22 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceIpmi.c). Their signatures and behavior are unverified; the no-parameter declarations exist for import resolution and throw through `NotImplemented_nid_no_patch`.

- [libSceIpmi pending exports](../../../core/libs/prx/libSceIpmi/Export.cpp) - 5 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceIpmi.c) and were cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

- [libSceIpmi no-op handlers](../../../core/libs/prx/libSceIpmi/PendingExports.cpp) - the `Session`, `Client`, `Server` and `Server::EventHandler` `D1`/`D2` destructors and the four `EventHandler` `onSessionKilled`/`onSyncMethodDispatch`/`onAsyncMethodDispatch` callbacks are implemented from the 11.00 module, where each body is a single `ret`; they take the object pointer and leave it untouched.

- [libSceIpmi config estimators and empty queries](../../../core/libs/prx/libSceIpmi/PendingExports.cpp) - the three memory estimators and the three non-blocking queries are implemented from the 11.00 module: `Session::Config::estimateSessionMemorySize` returns 0, `Client::Config::estimateClientMemorySize` returns 0x1000, `Server::Config::estimateTempWorkingMemorySize` returns `max(field0 + 0x100, field1 + 0x100)`, and `ClientImpl::tryGetResult` (`PKvm` overload), `ClientImpl::invokeAsyncMethod` (`PKvm` overload) and `SessionImpl::tryDispatch` return 0 without touching the object.

- [libSceIpmi config constructors](../../../core/libs/prx/libSceIpmi/PendingExports.cpp) - the meaning of the fields is unknown, and `this` is not checked for null, as in the module. The `Client::Config`, `MsgQueueConfig`, `SyncMethodInvocationConfig`, `AsyncMethodInvocationConfig` and `Server::Config` constructors write the defaults firmware 13.42's module writes (read from disassembly, not run): 1, 0xf00 and 0xf00 at 0x00, 0x08 and 0x10 for both invocation configs; 0 at 0x00 and 0x100 zero bytes from 0x08 for `MsgQueueConfig`; 0xf00, 0xf00 and 0 at 0x00, 0x08 and 0x10 plus 16-bit zeroes at 0x18 and 0x32 for `Server::Config`; and for `Client::Config` zero bytes at 0x00 and 0x19, 0 at 0x1c, invocation configs at 0x20 and 0x38 and a `MsgQueueConfig` at 0x50. Bytes they don't write keep their value. `MsgQueueConfig::estimateMsgQueueSize` returns `(size + 0x10) * count` in 64 bits.

### Known export names; ABI prototypes unverified

- 37 pending exports in [Export.cpp](../../../core/libs/prx/libSceIpmi/Export.cpp) are present in the [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs) inventory. Each is named after the mangled symbol the [verified NID name database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) gives for its NID, and the NID of that name matches. Their prototypes and behavior are unverified. They use the no-parameter import-resolution convention and throw through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.

### Additional callable exports


- [Additional callable exports](../../../core/libs/prx/libSceIpmi/Export.cpp) - 3 entries observed in the 3.20 export inventory and present in the later 13.60-labelled inventory are import-resolution declarations only. The later inventory label does not authenticate a retail firmware version. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `fjPNqzuUop8` |
| `Unknown` | `gL4bo5h2nwU` |
| `Unknown` | `rq3L3xKnt3Y` |
