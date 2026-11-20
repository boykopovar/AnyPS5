# libc

## add Functional

- [lgammal](../../../core/libs/prx/libc/src/MathGamma.cpp) has no host gamma fallback: moderate arguments use recurrence and rational corrections with split-log reconstruction, and twelve negative-root intervals use shifted polynomials. Infinity and NaN classification matches the bundled reference. Mathematical constants and operation order follow PPSA21564's libc (SHA-256 `ab860676108827e0983eefbde3a711b5ce8e20f0b24b492498b6e04cb23c6130`), characterized in a guarded CPU fixture. Interval edges, tiny values, recurrence boundaries, raw exception flags and caller precision/rounding are tested. Console, Windows, exhaustive input coverage and enabled-exception behavior in the new finite paths remain unverified; no title unblock or performance gain is established. Labels: tested, console
