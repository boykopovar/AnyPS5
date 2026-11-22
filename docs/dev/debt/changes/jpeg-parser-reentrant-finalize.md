# libSceJpegParser

## add Functional

- [JPEG parser initialization](../../../core/libs/prx/libSceJpegParser/Thumbnail.cpp) - Firmware behavior when the allocator calls Finalize during Initialize is unverified. The host returns the parser-state error from the canceled Initialize and releases any returned context.
