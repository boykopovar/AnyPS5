# libSceMoveTracker technical debt

### Unknown function info

- [libSceMoveTracker](../../../core/libs/prx/libSceMoveTracker/Export.cpp) remains unimplemented. Calibration direction, camera/controller input, capture initialization, state query, memory-size query and playback output signatures are recovered. Camera and controller records occupy 24 bytes, state records 160 bytes, and playback metadata 16 bytes; their complete field meanings still require reconstruction. Initialization consumes a work-memory pointer, a required nonzero 64-bit argument and two 32-bit GPU queue selectors below eight. The second argument is only checked for zero; its declared type remains unresolved. Capture/playback helper signatures need further checking against caller register use.
