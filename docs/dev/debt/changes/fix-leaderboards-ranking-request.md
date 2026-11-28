# libSceNpCppWebApi

## change [sce::Np::CppWebApi::Common]

- [sce::Np::CppWebApi::Common](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) (libSceNpCppWebApi) - the `finish` overrides of `DownStreamTransaction<...>` (2 instantiations) return success without doing work; a failed request never produces a transaction to finish. `Transaction<...>::setOnFinishedCallback` and `TransactionBase<...>::setResponseInformationOption` (5 instantiations each, the `UserProfile::V1`, `ProfanityFilter::V2` and `CommunicationRestrictionStatus::V3` responses) return success without storing the callback or the option: `start` fails, so neither is ever used. The constructors and destructors of the same five `Transaction<...>` instantiations succeed without keeping state

## change [sce::Np::CppWebApi::Leaderboards::V1]

- [sce::Np::CppWebApi::Leaderboards::V1](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) (libSceNpCppWebApi) - the unmeasured `RecordScoreRequestBody` setters discard their input. Its `setNpServiceLabel` and `setWaitsForData` store the value and presence flag. `BoardsApi::ParameterToGetBoardDefinition` allocates a host object with the board id and a `Common::String` at +0x8; `setfields` stores the string and +0x28 and returns 0x80553502 before `initialize` or 0x80553503/0x80553504 when the copy fails. `RecordApi::*` and `ViewApi::ParameterToGetLargeDataByObjectId` parameters keep only the initialized flag and return the PPSA21564 codes, including 0x80553512 for an empty `RecordScoreRequestBody`. Offsets are measured on PPSA21564/PPSA23732 (`J03093737`/`J03341606`); the host object uses the host allocator.

## add Functional

- [Leaderboards::V1 ranking request construction](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) creates and retains users, copies their vector into a ranking body, and retains that body in `ViewApi::ParameterToGetRanking`. Call `terminate` before destroying an initialized parameter. In PPSA28050, D1 leaves the implementation pointer and body reference untouched; `terminate` releases them. PPSA28050 supplies the observed field, reference-count and capacity layouts. The host allocates this storage. Guest custom-allocation callbacks, original source-level scalar return declarations and behavior on other firmware remain unverified. The export that submits the ranking request still reports the signed-out error.
