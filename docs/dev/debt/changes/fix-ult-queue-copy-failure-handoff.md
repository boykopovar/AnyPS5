# libSceUlt

## add Functional

- [Queue buffer copy failures](../../../core/libs/prx/libSceUlt/Export.cpp) (libSceUlt) - console behavior for unreadable push buffers, unwritable pop buffers and waiter selection is unmeasured. The host keeps its `ULT_ERROR_NULL` fallback; a failed transfer leaves its slot or item available and wakes another host waiter rather than consuming the only notification.
