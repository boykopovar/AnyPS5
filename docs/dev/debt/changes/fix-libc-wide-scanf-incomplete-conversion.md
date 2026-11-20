# libc

## add Functional

- [A wide scanf format that ends inside a directive](../../../core/libs/prx/libc/src/ScanningWide.cpp) (libc) - `swscanf(u"50%", u"%d%", ...)`, and a format that ends after a `*`, a width or a length modifier, stops as a matching failure and returns the assignments made so far, in `swscanf`, `swscanf_s` and the stream forms, as the narrow family does in [Formatting.cpp](../../../core/libs/prx/libc/src/Formatting.cpp). The console's wide scanner is not measured for it; the wide printf family, whose probe read past a trailing `%`, still rejects one.
