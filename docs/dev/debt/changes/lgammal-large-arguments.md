# libc

## add Functional

- [lgammal](../../../core/libs/prx/libc/src/MathGamma.cpp) is compiled with `-frounding-math` and uses a scaled Stirling expansion with reciprocal Bernoulli corrections for x >= 130, and sine/log reflection for negative non-integers below -130. `_LLog`, `_LSin`, `_LDunscale` and `_LDscale` retain caller precision; volatile temporaries retain reciprocal/log/argument/square evaluation order and the flags before an overflow return. A guarded CPU fixture of PPSA21564's bundled libc (SHA-256 `ab860676108827e0983eefbde3a711b5ce8e20f0b24b492498b6e04cb23c6130`) supplies numeric/flag reference cases, not console measurements. Remaining finite inputs still use host math and may differ; no full gamma conformance or title unblock is established. Labels: tested, console
