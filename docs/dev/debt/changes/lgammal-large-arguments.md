# libc

## add Functional

- [lgammal](../../../core/libs/prx/libc/src/MathGamma.cpp) uses `-frounding-math`, scaled Stirling corrections for x >= 130, and sine/log reflection for negative non-integers below -130. Extended-precision helpers and volatile intermediates preserve caller precision, evaluation order and overflow flags. Some finite negative inputs reproduce the reference's NaN/invalid results under directed rounding. Reference cases use a guarded native CPU fixture of PPSA21564's bundled libc (SHA-256 `ab860676108827e0983eefbde3a711b5ce8e20f0b24b492498b6e04cb23c6130`), not console measurements. Other finite inputs still use host math and may differ; full gamma conformance and title unblocks remain unproven. Labels: tested, console
