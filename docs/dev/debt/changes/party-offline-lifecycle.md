# Party

## change [Party](../../../core/libs/prx/Party/Export.cpp)

- [Party](../../../core/libs/prx/Party/Export.cpp) (Party) - 16 network, endpoint and descriptor functions that titles import from the `Party` module, taken from the imports of PPSA02632 (01.000.000). Each name is the PlayFab Party flat C function whose name hashes to the NID; all are declared in the pinned [Party_c.h](https://github.com/PlayFab/PlayFabParty/blob/dcf63eb9daf691bf6e0094652b0bb50c0a3b0535/include/Party_c.h). Which Party release a title bundles is not known, so the parameterless `int` declarations stay placeholders, not verified signatures, and each function throws through `NotImplemented_nid_no_patch`; how the library behaves without the PlayFab service is not documented or measured

## remove `PartySetMemoryCallbacks`

## remove `PartyInitialize`

## remove `PartyCleanup`

## remove `PartyStartProcessingStateChanges`

## remove `PartyFinishProcessingStateChanges`

## remove `PartyCreateLocalUser`

## remove `PartyGetLocalUsers`

## remove `PartyGetNetworks`

## remove `PartyLocalUserGetEntityId`

## remove `PartyLocalUserUpdateEntityToken`

## add Functional

- [Party lifecycle and local users](../../../core/libs/prx/Party/src/Manager.cpp) (Party) - offline only and not compared with a bundled Party library or a console: no state change is ever produced, the network list stays empty, the entity token is stored unvalidated and the memory callbacks are recorded but never called. Misuse that [Party.h](https://github.com/PlayFab/PlayFabParty/blob/dcf63eb9daf691bf6e0094652b0bb50c0a3b0535/include/Party.h) forbids (callbacks after another call or twice, a second initialize, a stale handle or user, unknown state changes, null arguments) throws, and a ninth local user throws through `NotImplemented_nid_no_patch`, because no error code but `c_partyErrorSuccess` (0) is public. Closes when: the error codes are measured from a bundled Party library.
