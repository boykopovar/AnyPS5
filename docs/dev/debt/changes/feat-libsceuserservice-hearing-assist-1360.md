# libSceUserService

## change [sceUserServiceGetUserWithHearingAssist]

- [sceUserServiceGetUserWithHearingAssist](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - 13.60 ABI is `(int32_t* userId)` and dispatches query `0x3002b`. Measured on a PS5 Pro payload process (13.60): a null destination answers `0x80960002` before initialization and `0x80960005` after it - the pointer check precedes any login state - and with a writable destination the initialized call answers 0 writing `-1`, an invalid user id, in a session-less payload. This implementation follows that order and answers `-1` always: no hearing-assist state exists here for a user to have enabled. Whether a console where some user has the setting enabled answers that user's id is unknown.
