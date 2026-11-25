# libc

## change [num_put<char>::do_put]

- [num_put<char>::do_put](../../../core/libs/prx/libc/src/LocaleSupport.cpp) (libc) follows the SDK 7.00 `libc.prx` that PPSA06416 ships, read from its disassembly and run, but formats through this library's `snprintf`. Its digits come from the extended-precision conversion that the titles' `snprintf` uses, so 1e10 at precision 22 is `9999999999.999999999999` and -2.5 at precision 0 is `-3`, as in that module; `do_put` itself has not been compared with it again. A precision so negative that the module's buffer of precision + 50 bytes cannot hold the result throws, where the module reads stale bytes. A stream locale without a `ctype<char>` facet leaves digits unwidened, as the console's C-locale facet does.
