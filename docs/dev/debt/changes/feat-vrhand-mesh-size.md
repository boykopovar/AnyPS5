# libSceVrHand

## add Functional

- [sceVrHandGetMeshSize](../../../core/libs/prx/libSceVrHand/Export.cpp) (libSceVrHand) - follows the [firmware 13.42 module reading](https://github.com/boykopovar/AnyPS5/issues/21881#issuecomment-6951072734): before initialization it returns `0x8a870001` without touching the output; otherwise null gives `0x8a870006`. For writable output it stores 32-bit counts 26, 2405 and 12816 at offsets 0, 4 and 8, clears bytes 12 through 25 inclusive and returns 0. The exact names of the fields are unverified. Reporting `0x8a870006` on host copy-out failure (possibly after a partial write) is a host protection, not measured console behavior. Closes when: verified on a PS5.
