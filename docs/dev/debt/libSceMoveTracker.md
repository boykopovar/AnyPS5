# libSceMoveTracker technical debt

### Silent stubs

- [sceMoveTrackerGetVersion and sceMoveTrackerRecordStop](../../../core/libs/prx/libSceMoveTracker/Export.cpp) return a constant version or success without tracking or recording state management.

### Functional

- [sceMoveTrackerInit](../../../core/libs/prx/libSceMoveTracker/Export.cpp) (libSceMoveTracker) - in firmware 13.42 libSceMoveTracker.sprx export F4w2atwG428, initialization consumes rdi workMemory != nullptr, nonzero rsi requiredArgument, edx pipeId < 8 and ecx queueId < 8; invalid arguments return `0x80ef0003`. The second argument is only checked for zero; its declared type remains unresolved. Past parameter validation, initialization remains unimplemented.

### Unknown function info

- [libSceMoveTracker](../../../core/libs/prx/libSceMoveTracker/Export.cpp) remains unimplemented. Calibration direction, camera/controller input, capture initialization, state query, memory-size query and playback output signatures are recovered. Camera and controller records occupy 24 bytes, state records 160 bytes, and playback metadata 16 bytes; their complete field meanings still require reconstruction. Capture/playback helper signatures need further checking against caller register use.
- [sceMoveTrackerGetWorkingMemorySize](../../../core/libs/prx/libSceMoveTracker/Export.cpp) (libSceMoveTracker) - what the two sizes cover is unknown beyond the parameter names. It follows firmware 13.42's libSceMoveTracker.sprx, read from its disassembly and run natively on x86-64 (not a console): a null pointer returns `0x80ef0003` and writes nothing; otherwise it stores the 32-bit values 0xa00000 and 0x800000 through the first and second pointer and returns 0. The first is module data 0x200000 plus the 0x800000 that an internal function returns; every rip-relative reference to that data in the module reads it.
