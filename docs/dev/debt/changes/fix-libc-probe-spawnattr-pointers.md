# libSceLibcInternal

## add Unknown function info

- [posix_spawnattr guest pointers](../../../core/libs/prx/libSceLibcInternal/Spawn.cpp) (libSceLibcInternal) - the attributes handle, the getter outputs and the setter inputs are read and written with checked guest copies; an unreadable or unwritable pointer throws `std::invalid_argument`, as the null and uninitialized cases already do, because whether the console faults or returns an error number on unmapped memory is unmeasured (`posix_spawnattr_init` frees the new attributes before throwing).
