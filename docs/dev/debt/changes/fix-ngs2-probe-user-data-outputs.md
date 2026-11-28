# libSceNgs2.native

## add Unknown function info

- [sceNgs2SystemGetUserData and sceNgs2RackGetUserData guest outputs](../../../core/libs/prx/libSceNgs2.native/src/System.cpp) (libSceNgs2.native) - the user data is written with the guest copy helper; an unwritable output throws the invalid-argument exception the null output already throws, because whether the console faults or returns an error on unmapped memory is unmeasured.
