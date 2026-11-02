# libSceS3da technical debt

### Unknown function info

- [sceS3daClose, sceS3daGetInfo, sceS3daOpen and sceS3daSendData](../../../core/libs/prx/libSceS3da/Export.cpp) remain unimplemented. Port arguments and transfer buffer/length are recovered from the native module. `GetInfo` forwards its output pointer to a kernel ioctl without accessing its fields; the information layout requires the kernel handler or a caller that interprets its output.
