# libc

## add Functional

- [lgammal](../../../core/libs/prx/libc/src/Math.cpp) classifies nonpositive integer poles, including signed zeros and large representable integers, with `_LDint` before calling host math: it raises divide-by-zero through `_Feraise`, sets guest errno to 34 and returns positive infinity without adding inexact or changing `signgam`. This follows the title-bundled PPSA21564 `libc.prx` (SHA-256 `ab860676108827e0983eefbde3a711b5ce8e20f0b24b492498b6e04cb23c6130`), characterized in a native CPU fixture, not on a console. Other inputs still use host `lgammal`; their last bits and exception flags can differ. Labels: tested, console
