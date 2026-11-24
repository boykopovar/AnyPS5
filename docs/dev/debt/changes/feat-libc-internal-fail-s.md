# libSceLibcInternal

## change `_Fac_tidy`

- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Fac_tidy`

## add Unknown function info

- [_Fail_s](../../../core/libs/prx/libSceLibcInternal/ConstraintHandler.cpp) (libSceLibcInternal) - the console module was not read. The `void(const char*, void*, int)` prototype follows Dinkumware `_Fail_s`, forwarding to `SonyInvokeConstraintHandler_nid_no_patch` to invoke the thread-safe installed constraint handler or the default handler. Closes when: compared with the console module.
