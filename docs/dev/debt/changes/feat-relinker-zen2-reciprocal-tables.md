# relinker

## change [1/sqrt(x) computed as SQRT then DIV]

- `--to-intel` [reciprocal stubs](../../../core/relinker/codegen/src/x86/ReciprocalLowering.cpp) for (V)RCPPS, (V)RSQRTPS, (V)RCPSS and (V)RSQRTSS read [tables](../../../core/relinker/codegen/src/x86/SharedStubData.cpp) measured on a desktop Zen 2 (Ryzen 7 3700X), not on a PS5; whether the PS5's CPU uses the same tables is unmeasured, and PPSA21564, whose identity quaternion became a NaN axis with Intel's approximation, has not been rerun. On that Zen 2 the legacy packed and 256-bit register-form stubs match the native instructions for all 2^32 inputs. A stub is about 400 bytes (800 for a 256-bit form), plus 24.8 KB of tables per image. FS and GS memory forms, and a site shorter than 5 bytes with no movable instruction after it (5 in PPSA21564, not recounted), keep the native instruction. Closes when: measured on a PS5.
