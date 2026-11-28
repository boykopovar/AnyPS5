# libSceNpMatching2

## remove `sceNpMatching2JoinRoomA`

## add Functional

- [sceNpMatching2JoinRoomA](../../../core/libs/prx/libSceNpMatching2/Export.cpp) (libSceNpMatching2) - checked like the other room requests: initialization, a null request or request id, then the leading room id (`INVALID_ROOM_ID`), then the context (`CONTEXT_NOT_FOUND`). The signature and the leading `roomId` of the request follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/np/np_matching2/np_matching2.cpp) and its `OrbisNpMatching2JoinRoomRequestA`; the order of the checks is copied from the sibling requests and was not measured for this call on a console. A request that passes them still throws: how an accepted join completes offline is unknown
