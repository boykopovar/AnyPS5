# libSceNpCppWebApi

## change [sce::Np::CppWebApi title-imported parameter bookkeeping]

- [sce::Np::CppWebApi title-imported parameter bookkeeping](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) (libSceNpCppWebApi) - the `SessionManager::V1` `GameSessionsApi` `ParameterTo*` constructors, destructors, `initialize` and `terminate`, the remaining `RequestGameSession*`/`Patch*RequestBody`/`RequestMatch*`/`Added*`/`Removed*`/`RequestPlayer*`/`RequestTeam*`/`RequestMember*`/`RequestResults*` setters, and the Common `IntrusivePtr`/`Vector`/`Transaction` constructors, destructors and bookkeeping calls over the new families succeed without keeping state or contacting the network. The values are never read because a request is never started. Seventeen scalar setters in the game and player session request models are measured separately.

## change [sce::Np::CppWebApi::SessionManager::V1 audited accessors, fourth batch]

- [sce::Np::CppWebApi::SessionManager::V1 audited accessors, fourth batch](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) - PPSA21564 build `J03093737` uses these offsets: `GameSessionPlayer` custom data/join timestamp at 0x80/0x50 and `customData1IsSet` 0x98; `GameSessionForRead` max players/created/supportedPlatforms at 0x5c/0x38/0x90 (flags 0x60/0x58/0xa8); `PlayerSessionPlayer` and `PlayerSessionSpectator` platform/account/custom data/join timestamp at 0x30/0x10/0x70/0x50 with `customData1IsSet` 0x88; `PlayerSessionForRead` max players/spectators/invitable type/created/member/supportedPlatforms at 0x5c/0x64/0x15c/0x38/0x70/0x90 (flags 0x60/0x68/0x58/0x88/0x30/0xa8), `getJoinDisabled` 0x89; `PlayerSessionMemberForRead` players/spectators at 0x10/0x30 (flags 0x28/0x48); `JoinedPlayerSessionWithPlatform::getSessionId` 0x10; `GetUsersAccountIdPlayerSessionsResponseBody::getPlayerSessions` 0x30. Unmeasured setters are no-ops. Calls/factories return `NOT_SIGNED_IN`.

## add Functional

- [SessionManager::V1 request scalar fields](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) copy the input property and set its presence byte as the module bundled with PPSA28050 does. Its 17 leaf setters leave the copied integer or zero-extended byte in EAX. The original C++ return declaration and behavior across other module versions are unverified. Each guest input is read before the output to support aliases. An unreadable reference or unwritable destination throws as a host fallback. These fields are local model state. The signed-out API and unmeasured string, vector and binary fields have separate behavior.
