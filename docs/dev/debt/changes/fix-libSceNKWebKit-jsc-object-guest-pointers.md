# libSceNKWebKit

## add Functional

- [JSC object and class guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/JscObject.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `JSClassCreate`, `OpaqueJSClass::~OpaqueJSClass`, `JSObjectMakeFunction`, `JSObjectMakeDeferredPromise`, `JSObjectMakeArray`/`Date`/`Error`/`RegExp`, `JSObjectCallAsFunction`/`Constructor`, and `JSObject*` exception output slots is unmeasured; exports validate required guest reads (`GuestArenaPrepareHostRead_nid_no_patch` + `GuestReadable_nid_no_patch`) before `GuestCopyIn_nid_no_patch`, guard guest writes with active `GuestArena::HostWrite` scopes and `GuestWritable_nid_no_patch` before `GuestCopyOut_nid_no_patch`, and report failed required guest-memory accesses through `NotImplementedAt_nid_no_patch`. Closes when: console behavior for invalid guest pointers in `JscObject` exports is verified on hardware.
