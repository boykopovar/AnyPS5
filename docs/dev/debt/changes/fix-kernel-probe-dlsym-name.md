# libkernel

## add Unknown function info

- [dlsym symbol name](../../../core/libs/prx/libkernel/Module/src/DynamicLoader.cpp) (libkernel) - the name is copied in with `CopyGuestString_nid_no_patch` (up to 64 KiB) before the lookup; an unreadable name returns null with a `dlerror` message like the empty name, because the console's behaviour on unmapped memory is unmeasured.
