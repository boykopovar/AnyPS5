# libSceAgcDriver

## add Functional

- [Read-only buffer device mirror](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestBufferMemory.cpp) (libSceAgcDriver) - a read-only buffer element of write-watched host-import memory, of at least `APS5_READONLY_MIRROR_MIN_KIB` (16 MiB) and at most 256 MiB, is read from a device-local copy on a discrete GPU. Builds of the same range share one copy, which is reused while `GuestMemory::UnchangedSince` holds for the whole range; any write to the range copies it in again in full instead of only the changed 64 KiB blocks. The copy takes VRAM equal to the element and counts against the staging shadow budget, the reusable copies of all staged regions together are capped at 1 GiB (past that the whole set is dropped and the next uses copy in again), address-based (BDA) builds and unwatched memory still read the import in place, and the copy is not made without a recorder or when the device or the staging shadow budget has no room
