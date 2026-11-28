# libSceIcu

## add Functional

- [uprv_deleteUObject](../../../core/libs/prx/libSceIcu/LegacyExports.cpp) (libSceIcu) - not checked against `libSceIcu.sprx` or on a console. An unreadable object or vtable and a null deleting-destructor slot throw, where ICU 68.2 dereferences them ([uobject.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uobject.cpp#L102-L105)).
