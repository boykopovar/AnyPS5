# libSceNpWebApi

## change [sceNpWebApiIntCreateRequest]

- [sceNpWebApiIntCreateRequest](../../../core/libs/prx/libSceNpWebApi/Export.cpp) (libSceNpWebApi) - creates a request like `sceNpWebApiCreateRequest`, as [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/0614ce324be092791b758d79f93d9f1ca5b10a81/src/core/libraries/np/np_web_api/np_web_api.cpp#L411-L432) does: a null group or path, or a negative method or one past PATCH, returns `INVALID_ARGUMENT` whatever the SDK version, then a content length without a type returns `INVALID_CONTENT_PARAMETER`. Its internal arguments are three pointers that shadPS4 accepts only when all are null, so a set field throws after the user context check. Not measured on a console
