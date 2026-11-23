# libSceJpegEnc

## add Functional

- [Packed Y8U8Y8V8 encoding](../../../core/libs/prx/libSceJpegEnc/Export.cpp) is unmeasured on a console. Input luma and chroma are unpacked directly into [TurboJPEG 3.2.0 YCbCr planes](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/c85e6b905bf237038faa936dab160ebfc5da0344/src/turbojpeg.h#L1768-L1794) without clipping to RGB. 4:2:2 keeps the source chroma samples; 4:2:0 averages adjacent rows with [libjpeg-turbo's alternating rounding bias](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/c85e6b905bf237038faa936dab160ebfc5da0344/src/jcsample.c#L265-L296). Odd edges repeat the final sample or row as needed for the plane layout. The test decodes an odd-sized, padded image back to YCbCr and checks all visible samples for both sampling modes; console rounding, quantization and stream bytes remain unverified.
