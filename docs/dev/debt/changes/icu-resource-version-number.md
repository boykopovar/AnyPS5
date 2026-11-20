# libSceIcu

## add Functional

- [Resource bundle version strings](../../../core/libs/prx/libSceIcu/ResourceBundle.cpp) follow ICU 78.1's `ures_getVersionNumber`, which calls `ures_getVersionNumberInternal` and borrows the cached version string from the native bundle. Only handles owned by the resource bundle adapter are supported. Closes when: the guest signature, supported handle types, and string lifetime are verified on a PS5.
