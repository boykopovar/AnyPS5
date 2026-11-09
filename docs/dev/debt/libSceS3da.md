# libSceS3da technical debt

### Functional

- [sceS3daOpen and sceS3daClose](../../../core/libs/prx/libSceS3da/Export.cpp): in 13.42 module disassembly (sha256 156e8eb0bb7f459006f73908a23054dd2d640cf22b6835f12a467c2818da13e2), ports are bounded to 0 and 1 with no kernel call. `sceS3daOpen` sets slot state and returns `port` or 0x81600006 if already open; `sceS3daClose` clears the slot and returns 0 or 0x81600002 if unopened. `sceS3daSendData` validates port <= 1, non-null data, and size within 1..0xA000 (0x81600002); `sceS3daGetInfo` validates port <= 1 (0x81600002). Both ioctl paths remain unimplemented placeholders throwing NotImplemented_nid_no_patch.

### Unknown function info

- [sceS3daGetInfo and sceS3daSendData](../../../core/libs/prx/libSceS3da/Export.cpp): transfer data and info queries forward to kernel ioctls (0x8018a601 and 0x8010a602). Kernel ioctl behavior and output structure layouts remain unmodelled.
