# libSceNKWebKit

## add Functional

- [JSC::WeakHandleOwner and JSC::TypedArrayController deleting destructors](../../../core/libs/prx/libSceNKWebKit/src/JscOwnerDestructors.cpp) (libSceNKWebKit) - not compared with a console module. Both destructors are empty in the pinned [WeakHandleOwner.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/heap/WeakHandleOwner.cpp#L34-L36) and [TypedArrayController.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/TypedArrayController.cpp#L32), so each deleting destructor only frees the object. `WeakHandleOwner` declares no allocator ([WeakHandleOwner.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/heap/WeakHandleOwner.h#L34-L40)), so its deleting destructor frees through the guest global `operator delete(void*)`; whether the module calls the sized `operator delete(void*, unsigned long)` instead is not known, and both free the same block. `TypedArrayController` derives from `RefCounted`, which is `WTF_MAKE_FAST_ALLOCATED` ([TypedArrayController.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/TypedArrayController.h#L37-L40), [RefCounted.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/RefCounted.h#L184-L185)), so its deleting destructor frees through `WTF::fastFree`. A null object throws. Closes when: compared with a console module.

## remove `_ZN3JSC15WeakHandleOwnerD0Ev`

## remove `_ZN3JSC20TypedArrayControllerD0Ev`
