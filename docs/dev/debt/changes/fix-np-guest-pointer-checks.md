# libSceNpWebApi2

## add Functional

- [sceNpWebApi2AddWebTraceTag, sceNpWebApi2PushEventCreatePushContext and sceNpWebApi2PushEventStartPushContextCallback guest pointers](../../../core/libs/prx/libSceNpWebApi2/Export.cpp) (libSceNpWebApi2) - an unreadable or unterminated (within 4096 bytes) trace tag, a null `pushContextId` for create and start, and an unreadable `pushContextId` for start answer `INVALID_ARGUMENT` 0x80553402 with `errno` kept. The start call therefore no longer returns 0 for every id as the entry above says. None of these answers is measured on a console; they follow the other exports of the library that refuse a null or unreadable output.
