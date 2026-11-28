# libSceMoveTracker

## add Functional

- [sceMoveTrackerInit pointer safety](../../../core/libs/prx/libSceMoveTracker/Export.cpp) (libSceMoveTracker) - `sceMoveTrackerInit` validates writability of guest `workMemory` pointer before evaluating initialization arguments, returning `0x80ef0003` for null or unwritable memory with `errno` preserved.
