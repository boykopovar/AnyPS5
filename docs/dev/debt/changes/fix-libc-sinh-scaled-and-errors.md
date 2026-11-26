# libc

## add Functional

- [_Sinh](../../../core/libs/prx/libc/src/Math.cpp) (libc) is the two-argument Dinkumware helper `y * sinh(x)` and is not measured for `double`. Its special cases are those of `_FSinh`, which follows the `libc.prx` titles ship: a NaN operand is returned, infinity times zero in either order is NaN with `errno` 33, and a zero or infinite operand otherwise gives `x * y`. A finite argument from 18.738 up goes through `_Exp` with the factor, as in `_Cosh`, so a large argument with a small factor stays finite, and a result that overflows from finite operands sets `errno` 34 as measured for `_Cosh`. Below 18.738 it is `y` times this library's `sinh`, not the console's own sequence, so the last bit can differ. Closes when: `_Sinh` is run on a console or against a title's `libc.prx`.
