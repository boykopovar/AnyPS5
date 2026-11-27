# libSceUsbd

## add Functional

- [sceUsbdHandleEventsLocked, sceUsbdWaitForEvent](../../../core/libs/prx/libSceUsbd/Export.cpp) (libSceUsbd) - a non-null `timeout` is read through `GuestCopyIn_nid_no_patch` before it is validated and before the event lock is checked; an unreadable `timeout` throws, as its console result is unknown. Closes when: measured on a PS5
