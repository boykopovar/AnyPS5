# libSceIcu

## change [ICU date-time pattern generator exports]

- [ICU date-time pattern generator exports](../../../core/libs/prx/libSceIcu/DateTimePatternGenerator.cpp) use ICU 78.1/CLDR source data, not measured console ICU 68.2 behavior; locale defaults, data versions and pattern results may differ. Host-invalid guest buffers/status pointers and unknown or closed handles throw. Fields outside 0..15, display widths outside 0..2, lengths below -1 and address overflow throw defensively; native indexing/fault behavior is not reproduced. Opaque handle tokens retain process-lifetime identity storage to prevent stale-pointer reuse; native generator storage is released after close and outstanding operations. Windows execution and console equivalence remain unverified. Closes when: compared with the console and platform limits verified.
