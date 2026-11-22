# libSceS3da technical debt

### Functional

- [sceS3daGetInfo and sceS3daSendData](../../../core/libs/prx/libSceS3da/Export.cpp) have no `/dev/s3da` device behind them: AnyPS5's kernel models no such device, so the library's descriptor stays -1 and both return -1 after their argument checks. What the device does with the data, what it writes into the info structure and that structure's layout are not known. The library side follows firmware 13.42 `libSceS3da.sprx` (sha256 `156e8eb0bb7f459006f73908a23054dd2d640cf22b6835f12a467c2818da13e2`), read from disassembly to every return, not measured on a console. The 13.42 library opens `/dev/s3da` from its module start and prints a line when that fails; here it is opened on the first `sceS3daGetInfo` or `sceS3daSendData` call and nothing is printed. Closes when: a `/dev/s3da` model exists and is measured against a console.
