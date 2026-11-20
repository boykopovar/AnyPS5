# Party technical debt

### Unknown function info

- [Party](../../../core/libs/prx/Party/Export.cpp) (Party) - 26 functions that titles import from the `Party` module, taken from the imports of PPSA02632 (01.000.000). Each name is the PlayFab Party flat C function whose name hashes to the NID; all 26 are declared in the pinned [Party_c.h](https://github.com/PlayFab/PlayFabParty/blob/dcf63eb9daf691bf6e0094652b0bb50c0a3b0535/include/Party_c.h). Which Party release a title bundles is not known, so the parameterless `int` declarations stay placeholders, not verified signatures, and each function throws through `NotImplemented_nid_no_patch`; how the library behaves without the PlayFab service is not documented or measured

| Name | NID |
| --- | --- |
| `PartySetMemoryCallbacks` | `0iEKNAvT600` |
| `PartyFinishProcessingStateChanges` | `2cELEBPdYQ0` |
| `PartyNetworkGetNetworkDescriptor` | `5xYxiOtOccA` |
| `PartyCleanup` | `6W+shIH315Q` |
| `PartyCreateLocalUser` | `7ilObq815O4` |
| `PartySerializeNetworkDescriptor` | `7iq7Hfs8HBk` |
| `PartyGetNetworks` | `7kwSdZNDyP0` |
| `PartyNetworkCreateEndpoint` | `AC1FtjqMCL0` |
| `PartyLocalUserGetEntityId` | `AJ3I035AjIM` |
| `PartyDeserializeNetworkDescriptor` | `AMJZhpC+siY` |
| `PartyEndpointGetDevice` | `aq13EdrGdVw` |
| `PartyEndpointSetCustomContext` | `BOW-t-9qJNI` |
| `PartyStartProcessingStateChanges` | `E9XkHowt+dY` |
| `PartyNetworkGetDeviceConnectionType` | `f3d6tchKw94` |
| `PartyNetworkAuthenticateLocalUser` | `GADjreszOgM` |
| `PartyInitialize` | `Giyg2xGrXDU` |
| `PartyEndpointIsLocal` | `JrVwjbBmqLQ` |
| `PartyNetworkLeaveNetwork` | `KC3y21wrqzc` |
| `PartyLocalUserUpdateEntityToken` | `nVzRzo3Yr+s` |
| `PartyConnectToNetwork` | `pnz21I6VjDs` |
| `PartyCreateNewNetwork` | `Qb8wS4GZUsw` |
| `PartyEndpointSendMessage` | `qk043QPsrZU` |
| `PartyNetworkGetEndpoints` | `QVQ-lvlaJZw` |
| `PartyGetLocalUsers` | `SjQdzD78RpM` |
| `PartyEndpointGetEntityId` | `uDGXwUKqi8s` |
| `PartyEndpointGetCustomContext` | `YdBvG5Myxzg` |
