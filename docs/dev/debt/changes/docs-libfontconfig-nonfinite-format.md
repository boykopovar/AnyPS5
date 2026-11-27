# libfontconfig

## add Functional

- [Negative NaN in formatted doubles](../../../core/libs/prx/libfontconfig/src/PatternFormat.cpp) (libfontconfig) - `FcPatternFormat` prints double and matrix values with the host C library's `%g`, as [Fontconfig 2.15.0 fcname.c](https://github.com/fontconfig/fontconfig/blob/72b9a48f57de6204d99ce1c217b5609ee92ece9b/src/fcname.c) does, so a NaN with the sign bit set prints `nan` on Windows (MinGW-w64 `__mingw_snprintf`, selected by `HostStandardStreams.hpp`) and `-nan` on Linux (glibc). Measured with the library's compile flags in a standalone program on WinLibs GCC and on glibc 2.39, not through the export. Positive NaN, both infinities and the finite values tried (`1e+10`, `1e-05`, `1e+100`, `0.1`, `1.23457e+08`, `-0`) print the same on both hosts. What the console prints for a negative NaN is unmeasured.
