# libSceLibcInternal

## add Unknown function info

- [posix_spawn_file_actions guest handle](../../../core/libs/prx/libSceLibcInternal/Spawn.cpp) (libSceLibcInternal) - the file-actions handle is read and, by `posix_spawn_file_actions_init`, written with checked guest copies; an unreadable or unwritable handle throws `std::invalid_argument`, as the null and uninitialized cases already do, because whether the console faults or returns an error number on unmapped memory is unmeasured (`init` frees the new actions before throwing). The `addopen` path is still read in place.
