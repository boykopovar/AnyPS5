# libSceIcu

## add Functional

- [`ucnv_setFromUCallBack` and `ucnv_setToUCallBack`](../../../core/libs/prx/libSceIcu/ConverterCallbacks.cpp) forward to ICU 78.1 with host-side callback adapters that need no `icudt` data: the four ICU callbacks are translated back to their guest wrappers, guest callback and context pairs survive a replacement, and a null converter is reported as `U_ILLEGAL_ARGUMENT_ERROR` through the status. Gap: a record is per converter and direction, its entry is emptied when the new action is null and filled by any other, no record is released while `ucnv_close` still throws, so the records of closed converters would leak, a host callback the library did not install is reported as a null action with its context rather than an error, an invalid result out of a guest callback is not validated, and no title or homebrew has called these exports on a PS5.
