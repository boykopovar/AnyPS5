# libSceHttp

## add Silent stubs

- [sceHttpSetEpollId](../../../core/libs/prx/libSceHttp/Export.cpp) (libSceHttp) - returns 0 for a live template, connection or request and keeps nothing: the epoll id and user argument are dropped, since no request reaches the network and no epoll event is ever delivered. An unknown id returns `ERROR_INVALID_ID` and a call before init `ERROR_BEFORE_INIT`. [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/network/http.cpp) likewise only logs and returns 0. `sceHttpGetEpollId` still throws. Not measured on a console.
