# libSceVideoOutSecondary

## change Additional callable exports
- [Additional callable exports](../../../core/libs/prx/libSceVideoOutSecondary/Export.cpp) - slot table allocation (`KJWkt9y7YC4`) and teardown (`s+isXKLpwS0`) follow 13.60 `libSceVideoOutSecondary.sprx` (`0x1350`, `0x1470`) with four 40-byte entries (`handle`, `state`, 32-byte payload), returning `nullptr` when full/duplicate/unreadable or `0x8029000B`/`0x80290009` on release; human symbol names and console hardware output remain unverified.
