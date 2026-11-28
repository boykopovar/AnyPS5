# libSceUserService

## add Functional

- [user-group-name-output](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - inaccessible non-null guest pointers in this path are rejected with `USER_SERVICE_ERROR_INVALID_ARGUMENT` through checked guest copies. This host-safety fallback is covered by guest regression tests but has not been measured on a console. Closes when: measured on a PS5. Labels: console, tested.
