# libc

## add Functional

- [ilogb and ilogbf](../../../core/libs/prx/libc/src/Math.cpp) (libc) read the exponent from the bits instead of asking the host: with denormals-are-zero set in MXCSR a subnormal argument compared equal to zero and came back as `INT_MIN`, where the `libc.prx` bundled with PPSA10122 01.101.000 returns its exponent (0x4a5f0, 0x4a680); a subnormal raised the denormal flag and a signaling NaN `FE_INVALID`, where that module raises nothing. Results and flags equal the module's on 300,000 inputs per function in the four rounding modes and with DAZ, FTZ and both, run natively on Windows; not measured on a console. `ilogbl` still goes to the host and raises flags for zero that the module does not.
